#include "config.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "log.h"

namespace talpatch {

// 内置注入目标（对应旧版 scope.list，system_server 由 serverSpecialize 处理）
static const char *kBuiltinScopes[] = {
        "com.android.systemui",
        "com.android.shell",
        "com.android.packageinstaller",
        "moe.shizuku.privileged.api",
        "com.tal.pad.studyservice",
        "com.tal.dataupload",
        "com.tal.pad.backdoor",
        "com.tal.backdoor",
        "com.tal.pad.onlineclass",
        "com.tal.pad.usercenter",
        "com.tal.pad.minor_protect",
        "com.tal.pad.znxxservice",
};

// native 反检测 hook 的适用进程：TAL 自身（含其子进程）。
// 这些进程才会扫描 /proc/self/maps 做注入检测，其他进程包括 SystemUI、
// system_server 与普通应用都不需要，安装后反而会破坏其正常文件读取。
static const char *kNativeHookScopes[] = {
        "com.tal.pad.studyservice",
        "com.tal.dataupload",
        "com.tal.pad.backdoor",
        "com.tal.backdoor",
        "com.tal.pad.onlineclass",
        "com.tal.pad.usercenter",
        "com.tal.pad.minor_protect",
        "com.tal.pad.znxxservice",
};

std::vector<uint8_t> read_file(const char *path) {
    std::vector<uint8_t> out;
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOGW("read_file: open %s failed", path);
        return out;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 0 && size < (64L << 20)) {
        out.resize((size_t) size);
        size_t n = fread(out.data(), 1, out.size(), f);
        out.resize(n);
    }
    fclose(f);
    return out;
}

// 极简解析 "scope_extra": ["a.b.c", ...] 或 "a.b.c, d.e.f"
static std::vector<std::string> parse_extra_scopes(const std::string &json) {
    std::vector<std::string> out;
    size_t key = json.find("\"scope_extra\"");
    if (key == std::string::npos) return out;
    size_t colon = json.find(':', key);
    if (colon == std::string::npos) return out;
    size_t pos = colon + 1;
    while (pos < json.size() && isspace(json[pos])) pos++;
    if (pos >= json.size()) return out;
    if (json[pos] == '[') {
        size_t end = json.find(']', pos);
        if (end == std::string::npos) return out;
        size_t cur = pos;
        while (true) {
            size_t q1 = json.find('"', cur);
            if (q1 == std::string::npos || q1 >= end) break;
            size_t q2 = json.find('"', q1 + 1);
            if (q2 == std::string::npos || q2 > end) break;
            out.push_back(json.substr(q1 + 1, q2 - q1 - 1));
            cur = q2 + 1;
        }
    } else if (json[pos] == '"') {
        size_t q2 = json.find('"', pos + 1);
        if (q2 == std::string::npos) return out;
        std::string list = json.substr(pos + 1, q2 - pos - 1);
        size_t start = 0;
        while (start <= list.size()) {
            size_t comma = list.find(',', start);
            std::string item = list.substr(start,
                    comma == std::string::npos ? std::string::npos : comma - start);
            size_t b = item.find_first_not_of(" \t");
            size_t e = item.find_last_not_of(" \t");
            if (b != std::string::npos) out.push_back(item.substr(b, e - b + 1));
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }
    return out;
}

// 极简解析 "key": true/false
static bool parse_bool(const std::string &json, const char *name, bool def) {
    std::string key = std::string("\"") + name + "\"";
    size_t k = json.find(key);
    if (k == std::string::npos) return def;
    size_t colon = json.find(':', k + key.size());
    if (colon == std::string::npos) return def;
    size_t v = colon + 1;
    while (v < json.size() && isspace((unsigned char) json[v])) v++;
    if (json.compare(v, 4, "true") == 0) return true;
    if (json.compare(v, 5, "false") == 0) return false;
    return def;
}

// 提取 "key": {...} 的对象原文（值不是对象则返回空）
static std::string extract_object(const std::string &json, const char *name) {
    std::string key = std::string("\"") + name + "\"";
    size_t k = json.find(key);
    if (k == std::string::npos) return "";
    size_t colon = json.find(':', k + key.size());
    if (colon == std::string::npos) return "";
    size_t b = colon + 1;
    while (b < json.size() && isspace((unsigned char) json[b])) b++;
    if (b >= json.size() || json[b] != '{') return "";
    int depth = 0;
    bool in_str = false;
    for (size_t i = b; i < json.size(); ++i) {
        char c = json[i];
        if (in_str) {
            if (c == '\\') { ++i; continue; }
            if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') in_str = true;
        else if (c == '{') depth++;
        else if (c == '}' && --depth == 0) return json.substr(b, i - b + 1);
    }
    return "";
}

ModuleConfig load_module_config(const char *path) {
    ModuleConfig cfg;
    auto bytes = read_file(path);
    if (!bytes.empty()) {
        cfg.raw_json.assign(bytes.begin(), bytes.end());
        cfg.extra_scopes = parse_extra_scopes(cfg.raw_json);
        cfg.notify_all_enabled =
                parse_bool(cfg.raw_json, "notify_all_enabled", true);
        cfg.notify_app_overrides =
                extract_object(cfg.raw_json, "notify_app_overrides");
        LOGI("config loaded: %zu bytes, %zu extra scopes, notify_all=%d",
             cfg.raw_json.size(), cfg.extra_scopes.size(),
             cfg.notify_all_enabled ? 1 : 0);
    } else {
        LOGW("config missing, using defaults");
    }
    return cfg;
}

static bool name_matches(const char *nice_name, const char *entry) {
    size_t len = strlen(entry);
    if (len == 0) return false;
    if (strncmp(nice_name, entry, len) != 0) return false;
    return nice_name[len] == '\0' || nice_name[len] == ':';
}

bool is_target_process(const char *nice_name, const ModuleConfig &cfg) {
    if (!nice_name) return false;
    for (const char *entry : kBuiltinScopes) {
        if (name_matches(nice_name, entry)) return true;
    }
    for (const auto &entry : cfg.extra_scopes) {
        if (name_matches(nice_name, entry.c_str())) return true;
    }
    return false;
}

// 受限/隔离进程不应注入：app_zygote 的 SELinux 域无权访问 servicemanager
// binder，注入的 Java 代码一旦尝试 binder 调用就会被拒绝，导致 Momo 等检测
// 应用的隔离进程卡死、主界面一直等待（"服务无响应"）。
bool is_excluded_process(const char *nice_name) {
    if (!nice_name) return true;
    std::string n(nice_name);
    static const char *kSuffix = "_zygote";
    const size_t sl = strlen(kSuffix);
    if (n.size() > sl && n.compare(n.size() - sl, sl, kSuffix) == 0) {
        return true;
    }
    if (n.find(":sandboxed_process") != std::string::npos) {
        return true;
    }
    return false;
}

bool is_native_hook_process(const char *nice_name) {
    if (!nice_name) return false;
    for (const char *entry : kNativeHookScopes) {
        if (name_matches(nice_name, entry)) return true;
    }
    return false;
}

bool notify_enabled_for(const char *nice_name, const ModuleConfig &cfg) {
    if (!nice_name) return false;
    std::string proc = nice_name;
    size_t colon = proc.find(':');
    std::string pkg = colon == std::string::npos ? proc : proc.substr(0, colon);
    const std::string &ov = cfg.notify_app_overrides;
    if (!ov.empty()) {
        // 覆盖表为纯平铺 {"pkg":bool,...}，包名仅含 [a-z0-9_.]，直接子串匹配
        std::string needle = "\"" + pkg + "\"";
        size_t pos = ov.find(needle);
        if (pos != std::string::npos) {
            size_t c = ov.find(':', pos + needle.size());
            if (c != std::string::npos) {
                size_t v = c + 1;
                while (v < ov.size() && isspace((unsigned char) ov[v])) v++;
                if (ov.compare(v, 4, "true") == 0) return true;
                if (ov.compare(v, 5, "false") == 0) return false;
            }
        }
    }
    return cfg.notify_all_enabled;
}

} // namespace talpatch
