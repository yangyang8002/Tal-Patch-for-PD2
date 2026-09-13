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
#include <unistd.h>

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
        bool target = talpatch::is_target_process(nice, config_);
        // 非目标进程：若"应用消息通知"对该包生效，也注入（仅安装通知 hook）
        bool notify = !target && talpatch::notify_enabled_for(nice, config_);
        if (target || notify) {
            target_process_ = nice;
            dex_ = talpatch::read_file(kLoaderDex);
            LOGI("%s app process: %s (dex %zu bytes)",
                 target ? "target" : "notify-only", nice, dex_.size());
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


