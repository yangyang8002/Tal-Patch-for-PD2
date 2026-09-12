/*
 * TAL-Patch Zygisk 模块入口。
 *
 * 工作流程：
 *  preAppSpecialize / preServerSpecialize（进程仍有 root 权限）
 *      → 判断是否为注入目标、读取 /data/adb/modules/tal_patch 下的
 *        config/config.json 与 loader.dex（该目录普通进程无权访问）
 *  postAppSpecialize / postServerSpecialize
 *      → 安装 native 防检测 hook，经 InMemoryDexClassLoader 加载 dex
 *        并调用 com.kevin233.talpad.hook.HookEntry.init()
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "zygisk.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "config.hpp"
#include "inject.hpp"
#include "log.h"
#include "native_hooks.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

namespace {

constexpr const char *kModuleDir = "/data/adb/modules/tal_patch";
constexpr const char *kLoaderDex = "/data/adb/modules/tal_patch/loader.dex";

// 调试标记：经 root companion 进程写文件（zygote 上下文受 sepolicy 限制无法
// 直接写 /data/adb）。验证后可删。
static void write_debug_marker(zygisk::Api *api, const char *name) {
    if (api == nullptr) return;
    int fd = api->connectCompanion();
    if (fd < 0) return;
    write(fd, name, strlen(name));
    close(fd);
}

class TalPatchModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api_ = api;
        this->env_ = env;
        LOGI("module loaded");
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) {
            return;
        }
        const char *nice = env_->GetStringUTFChars(args->nice_name, nullptr);
        config_ = talpatch::load_module_config();
        if (talpatch::is_target_process(nice, config_)) {
            write_debug_marker(api_, (std::string("app_") + nice).c_str());
            target_process_ = nice;
            dex_ = talpatch::read_file(kLoaderDex);
            LOGI("target app process: %s (dex %zu bytes)", nice, dex_.size());
        } else {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
        env_->ReleaseStringUTFChars(args->nice_name, nice);
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (target_process_.empty()) {
            return;
        }
        talpatch::install_native_hooks();
        talpatch::inject_loader(env_, target_process_.c_str(),
                                config_.raw_json, std::move(dex_));
        dex_.clear();
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        config_ = talpatch::load_module_config();
        dex_ = talpatch::read_file(kLoaderDex);
        inject_server_ = !dex_.empty();
        write_debug_marker(api_, "system_server_pre");
        LOGI("system_server specialize, dex %zu bytes", dex_.size());
        if (!inject_server_) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postServerSpecialize(const ServerSpecializeArgs *args) override {
        if (!inject_server_) {
            return;
        }
        talpatch::install_native_hooks();
        talpatch::inject_loader(env_, "system_server",
                                config_.raw_json, std::move(dex_));
        dex_.clear();
    }

private:
    Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    talpatch::ModuleConfig config_;
    std::string target_process_;
    std::vector<uint8_t> dex_;
    bool inject_server_ = false;
};

} // namespace

REGISTER_ZYGISK_MODULE(TalPatchModule)

// root companion：接收进程名并写调试标记文件
static void companion_handler(int client) {
    char name[128] = {0};
    ssize_t n = read(client, name, sizeof(name) - 1);
    close(client);
    if (n <= 0) return;
    name[n] = 0;
    for (char *c = name; *c; ++c) {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '_' || *c == '.')) {
            *c = '_';
        }
    }
    mkdir("/data/adb/modules/tal_patch/debug", 0755);
    char path[256];
    snprintf(path, sizeof(path),
             "/data/adb/modules/tal_patch/debug/%s.marker", name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%ld\n", (long) time(nullptr));
        write(fd, buf, len);
        close(fd);
    }
}
REGISTER_ZYGISK_COMPANION(companion_handler)
