/*
 * native 层防检测：拦截 fopen/fgets/fclose，
 * 对读取 /proc/*\/maps、/proc/self/mounts 等文件的进程抹除敏感行。
 * 相比 Java 层 hook，对 checkInjectSoInfo / maps 扫描类检测更有效。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <set>

#include "dobby.h"

#include "log.h"
#include "native_hooks.hpp"

namespace talpatch {

// 命中即抹除的敏感关键字（全小写匹配）
static const char *kDeniedSubstrings[] = {
        "talpatch", "tal_patch", "tal-patch",
        "zygisk", "lsposed", "lspd", "riru",
        "magisk", "kernelsu", "/ksu", "ksud",
        "libtalpatch.so",
};

static FILE *(*orig_fopen)(const char *, const char *) = nullptr;
static char *(*orig_fgets)(char *, int, FILE *) = nullptr;
static int (*orig_fclose)(FILE *) = nullptr;

static std::set<FILE *> g_scrub_streams;
static std::mutex g_mutex;

static bool is_sensitive_path(const char *path) {
    if (!path) return false;
    // /proc/xxx/maps、/proc/self/mounts、smaps
    return strstr(path, "/proc/") != nullptr &&
           (strstr(path, "maps") || strstr(path, "mounts"));
}

static FILE *fake_fopen(const char *path, const char *mode) {
    FILE *f = orig_fopen(path, mode);
    if (f && is_sensitive_path(path)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_scrub_streams.insert(f);
    }
    return f;
}

static bool should_scrub(const char *line) {
    // 对每个敏感词做大小写不敏感的子串匹配
    for (const char *deny : kDeniedSubstrings) {
        size_t n = strlen(deny);
        for (const char *s = line; *s; ++s) {
            size_t i = 0;
            while (i < n && s[i] &&
                   (char) tolower((unsigned char) s[i]) == deny[i]) {
                i++;
            }
            if (i == n) return true;
        }
    }
    return false;
}

static char *fake_fgets(char *buf, int size, FILE *stream) {
    // 循环读取直到拿到一行不敏感的数据（敏感行直接吞掉）
    while (true) {
        char *ret = orig_fgets(buf, size, stream);
        if (!ret) return ret;
        std::lock_guard<std::mutex> lock(g_mutex);
        bool tracked = g_scrub_streams.count(stream) > 0;
        if (!tracked) return ret;
        if (!should_scrub(buf)) return ret;
        // 吞掉敏感行，继续读下一行
    }
}

static int fake_fclose(FILE *stream) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_scrub_streams.erase(stream);
    }
    return orig_fclose(stream);
}

void install_native_hooks() {
    static bool installed = false;
    if (installed) return;
    installed = true;
    void *sym_fopen = dlsym(RTLD_DEFAULT, "fopen");
    void *sym_fgets = dlsym(RTLD_DEFAULT, "fgets");
    void *sym_fclose = dlsym(RTLD_DEFAULT, "fclose");
    if (!sym_fopen || !sym_fgets || !sym_fclose) {
        LOGE("resolve libc symbols failed");
        return;
    }
    if (DobbyHook(sym_fopen, (dobby_dummy_func_t) &fake_fopen,
                  (dobby_dummy_func_t *) &orig_fopen) != 0) {
        LOGW("hook fopen failed");
    }
    if (DobbyHook(sym_fgets, (dobby_dummy_func_t) &fake_fgets,
                  (dobby_dummy_func_t *) &orig_fgets) != 0) {
        LOGW("hook fgets failed");
    }
    if (DobbyHook(sym_fclose, (dobby_dummy_func_t) &fake_fclose,
                  (dobby_dummy_func_t *) &orig_fclose) != 0) {
        LOGW("hook fclose failed");
    }
    LOGI("native anti-detect hooks installed");
}

} // namespace talpatch
