# TAL-Patch (Zygisk + WebUI)

针对学而思学习机（TAL / XPad，Android 13+）定制系统的解封模块：
**Zygisk 注入 + KernelSU WebUI 配置**，取代旧版 LSPosed 插件形态，注入更隐蔽、抗检测能力更强。

## 功能

| 功能 | 说明 | 默认 |
|---|---|---|
| 解除安装限制 | 绕过安装白名单、未知来源与 V 型设备限制（system_server + packageinstaller） | 开 |
| 屏蔽用户中心环境检测 | root / hook 框架 / 模拟器 / 代理 / 调试检测与设备上报全部失效 | 开 |
| 恢复通知内容 | 还原被 TAL 系统隐藏的通知标题、正文、进度条与操作按钮 | 开 |
| 应用粒度通知开关 | 扫描全机应用（用户+系统），默认全部恢复通知；可搜索并逐包开关，改动后可一键重启变更应用 | 全开 |
| 阻止恢复默认壁纸/桌面 | 防止系统重置壁纸与默认 Launcher | 关 |
| 自定义学习/应用使用时长 | 拦截真实统计，按配置伪装上报（每日一次、24h 上限） | 关 |
| 伪装学习时长上报 | 拦截 `xpad_study_duration` 真实上报并替换 | 开 |
| 禁用未成年人精细化管控 | 禁玩指令、应用管控失效，SLS/神策埋点屏蔽 | 开 |
| 限制 Root 痕迹日志 | `SystemProperties`/`File`/`PackageManager` 过滤 su/magisk/ksu/zygisk 痕迹 | 开 |
| native 防检测 | Dobby inline hook `fopen`/`fgets`，抹除 `/proc/*/maps` 中模块与 root 特征 | 开 |

## 与旧版（LSPosed）的区别

- **注入器**：LSPosed(lspd) → 自带 Zygisk 模块（`zygisk/jni/module.cpp`），进程 `specialize` 阶段注入；
- **Java Hook**：libxposed API → **LSPlant**（`HookBridge`/`NativeBridge` 保持原链式 API 不变）；
- **native Hook**：新增 **Dobby**（maps 过滤等），对抗 `checkInjectSoInfo` 类 native 检测；
- **配置 UI**：Android App → **KernelSU WebUI**，内置 **MIUIX / Material You 双主题**可切换；
- **配置通道**：XposedService/RemotePreferences → `specialize` root 窗口期读取
  `/data/adb/modules/tal_patch/config/config.json` + `/sdcard/TAL-Patch/config.json` 镜像轮询热更新；
- **跨进程状态**：`openRemoteFile` → `/sdcard/TAL-Patch/state/` 共享小文件（`StateStore`）。

## 目录结构

```
├── loader/                 # Hook 逻辑（Java），编译后经 d8 打成 loader.dex
│   └── src/main/java/com/kevin233/talpad/hook/
│       ├── HookEntry.java      # 全部 hook 逻辑（init 由 native 调用）
│       ├── HookBridge.java     # 链式 Hook API（Hooker/Chain/proceed）
│       ├── NativeBridge.java   # JNI 声明 + LSPlant 回调入口
│       ├── ConfigBridge.java   # 配置读取（注入 JSON + /sdcard 镜像热更）
│       ├── CustomUsage.java    # 学习/应用时长配置解析
│       ├── RootTraceHider.java # Root 痕迹过滤规则
│       ├── SdcardLog.java      # 日志
│       └── ...
├── zygisk/                 # Zygisk 注入器（C++/CMake）
│   └── jni/
│       ├── module.cpp          # zygisk::ModuleBase：specialize 注入调度
│       ├── inject.cpp          # InMemoryDexClassLoader 内存加载 dex
│       ├── bridge.cpp          # LSPlant glue（nativeHook/deoptimize）
│       ├── native_hooks.cpp    # Dobby maps 过滤
│       └── config.cpp          # 配置读取与注入目标匹配
├── module/                 # KernelSU/Magisk 模块模板
│   ├── module.prop
│   ├── customize.sh / service.sh
│   ├── config.default.json
│   └── webroot/              # WebUI（index.html + MIUIX/Material 双主题）
├── build.py                # 打包脚本（产出可刷入 zip）
└── .github/workflows/build.yml
```

## 构建

```bash
# 依赖：JDK 17、Android SDK 34、NDK r26+、CMake 3.28~3.31（DexBuilder 要求 ≥3.28，
# Dobby 与 CMake 4.x 不兼容；可 pip install "cmake==3.31.*" ninja）
# 首次构建自动拉取 LSPlant v6.4 / Dobby / Zygisk API 头文件
./gradlew :loader:assembleRelease   # Java → classes.jar
python build.py                     # CMake 构建 libtalpatch.so + d8 打 dex + 打包
                                    # → dist/tal_patch-v26.9.1.zip（--skip-native 可跳过 native 构建）
```

也可以直接推送 tag，由 GitHub Actions 构建并发布。

## 安装与使用

1. 设备已刷入 **KernelSU** 并启用 **Zygisk**（推荐 **ZygiskNext**，实测 1.5.0 可用，
   应用进程与 system_server 均正常注入）；
2. KernelSU 管理器 → 模块 → 刷入 `tal_patch-v26.9.1.zip` → 重启；
3. KernelSU 管理器 → 模块 → TAL-Patch → **WebUI** 进行配置（右上角可切换 MIUIX / Material 主题）；
4. 配置保存后数秒内热更新生效；涉及「附加作用域」等注入目标的变更需强停目标应用或重启。

> 真机验证：TALIH-PD2（学而思 XPad，Android 13，KernelSU 35071 + ZygiskNext 1.5.0），
> 应用进程（studyservice/znxxservice/systemui/shell）与 system_server（PMS 安装白名单等）
> 全部 hook 生效。

## 致谢与署名

- **Kevin233** —— 原 TAL-Patch 作者，全部 hook 点逆向与逻辑实现  
  <https://github.com/Kevin233B>
- **yangyang8002** —— Zygisk 注入器、KernelSU WebUI（双主题）与防检测重构  
  <https://github.com/yangyang8002>

感谢 [LSPlant](https://github.com/LSPosed/LSPlant)、[Dobby](https://github.com/jmpews/Dobby)、
[Zygisk](https://github.com/topjohnwu/zygisk-module-sample) 与 [KernelSU](https://github.com/tiann/KernelSU)。

## 许可证

[CC BY-NC-SA 4.0](LICENSE)（署名-非商业性使用-相同方式共享，**禁止商业使用**）

> 本项目仅供学习与研究用途，使用产生的后果由使用者自行承担。
