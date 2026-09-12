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

// 调试标记：在 root 权限窗口期向模块目录写文件，用于验证注入路径
//（logcat 早期日志易丢失，文件标记是权威证据）。验证后可删。
static void write_debug_marker(const char *name) {
    char path[256];
    snprintf(path, sizeof(path),
             "/data/adb/modules/tal_patch/debug/%s.marker", name);
    mkdir("/data/adb/modules/tal_patch/debug", 0755);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "%ld
", (long) time(nullptr));
        write(fd, buf, n);
        close(fd);
    }
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
            write_debug_marker((std::string("app_") + nice).c_str());
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
        write_debug_marker("system_server_pre");
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
// 无 companion 进程需求，不注册 REGISTER_ZYGISK_COMPANION
