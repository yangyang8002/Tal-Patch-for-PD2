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

ModuleConfig load_module_config(const char *path) {
    ModuleConfig cfg;
    auto bytes = read_file(path);
    if (!bytes.empty()) {
        cfg.raw_json.assign(bytes.begin(), bytes.end());
        cfg.extra_scopes = parse_extra_scopes(cfg.raw_json);
        LOGI("config loaded: %zu bytes, %zu extra scopes",
             cfg.raw_json.size(), cfg.extra_scopes.size());
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

} // namespace talpatch
