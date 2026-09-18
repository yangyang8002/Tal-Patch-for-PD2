#pragma once

#include <string>
#include <vector>

namespace talpatch {

struct ModuleConfig {
    std::string raw_json;                 // 原始配置 JSON，注入时传给 Java 层
    std::vector<std::string> extra_scopes; // 配置中 scope_extra 指定的附加注入包名
    bool notify_all_enabled = true;        // 全局默认：对所有应用恢复通知内容
    std::string notify_app_overrides;      // 应用粒度覆盖表（原样 JSON 字符串）
};

// 在 specialize 阶段（进程仍有 root 权限）调用，读取模块配置。
ModuleConfig load_module_config(
        const char *path = "/data/adb/modules/tal_patch/config/config.json");

// 判断进程名是否应注入（内置列表 + extra_scopes），"pkg:sub" 子进程按 pkg 匹配。
bool is_target_process(const char *nice_name, const ModuleConfig &cfg);

// 应用粒度消息通知：该包是否应恢复通知内容（默认取 notify_all_enabled，
// notify_app_overrides 中的 "pkg":true/false 覆盖默认值）。"pkg:sub" 按 pkg 匹配。
bool notify_enabled_for(const char *nice_name, const ModuleConfig &cfg);

// 是否需要安装 native 反检测 hook（libc fopen/fgets/fclose 抹除 maps 痕迹）。
// 仅在会被 TAL 反作弊扫描的进程安装，绝不能装到 system_server / SystemUI /
// 普通应用进程：inline hook libc 会破坏其文件读取，导致系统与第三方应用异常。
bool is_native_hook_process(const char *nice_name);

// 读取整个文件（loader.dex / config.json 用），仅可在 root 权限窗口期调用。
std::vector <uint8_t> read_file(const char *path);

} // namespace talpatch
