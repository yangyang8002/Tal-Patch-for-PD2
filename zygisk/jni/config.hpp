#pragma once

#include <string>
#include <vector>

namespace talpatch {

struct ModuleConfig {
    std::string raw_json;                 // 原始配置 JSON，注入时传给 Java 层
    std::vector<std::string> extra_scopes; // 配置中 scope_extra 指定的附加注入包名
};

// 在 specialize 阶段（进程仍有 root 权限）调用，读取模块配置。
ModuleConfig load_module_config(
        const char *path = "/data/adb/modules/tal_patch/config/config.json");

// 判断进程名是否应注入（内置列表 + extra_scopes），"pkg:sub" 子进程按 pkg 匹配。
bool is_target_process(const char *nice_name, const ModuleConfig &cfg);

// 读取整个文件（loader.dex / config.json 用），仅可在 root 权限窗口期调用。
std::vector <uint8_t> read_file(const char *path);

} // namespace talpatch
