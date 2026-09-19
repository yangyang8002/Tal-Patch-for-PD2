# 更新日志

## v26.9.4

### 新增
- **设备信息伪装（QQ / TIM）**：借鉴 TCQT 的 CustomDevice 思路，在 QQ / TIM
  进程内 hook `SystemProperties#get` 与腾讯 `DeviceInfoMonitor#getModel`，
  把 `ro.product.device/model/manufacturer` 伪装成正常手机指纹，规避因学习机
  设备信息（alps / TALIH-PD2 / ls12_mt8797_wifi_64）被腾讯判定为风险设备。
  - WebUI「高级」页新增开关与设备代号/型号/制造商三项配置
  - 默认示例：device=ingres、model=21121210C、manufacturer=Xiaomi
  - 注意：需先把 QQ 移出 ZygiskNext 排除列表，否则模块无法注入 QQ


## v26.9.3

### 修复
- **修复 Momo（Mahoshojo）等检测类应用"服务无响应"**：模块默认全量注入时
  会把应用的 `_zygote` 隔离进程（SELinux 域 `app_zygote`）也注入。该域无权
  访问 servicemanager binder，注入代码反复尝试 binder 调用被 SELinux 拒绝，
  导致隔离检测进程卡死，主界面一直转圈。
- 注入时排除 `*_zygote` 与 `:sandboxed_process` 等受限进程。


## v26.9.2（重要修复）

### 修复
- **修复模块导致其他应用无法启动、桌面图标丢失的严重问题**：
  此前 native 反检测 hook（Dobby inline hook libc `fopen`/`fgets`/`fclose`）
  被安装到了**所有注入进程**，包括 `system_server`、`SystemUI` 与全部普通
  应用，破坏其正常文件读取。现收紧为**仅在会被 TAL 反作弊扫描的进程**
  （学而思自身若干服务进程）安装。
- native hook 增加空指针与失败防御：`orig_*` 未就绪时不再调用，避免崩溃。
- `system_server` 彻底不再安装 native hook。

### 建议
- 从 v26.9.1 升级后请重启一次设备。

## v26.9.1

首个正式发布版本（Zygisk + KernelSU WebUI 架构）。

### 新增
- 全面重构为 **Zygisk 注入 + KernelSU WebUI** 模块，不再依赖 LSPosed。
- **应用级消息通知**：扫描机内全部应用（用户 + 系统），默认全部启用；
  支持按包名/应用名搜索、图标展示、筛选（全部/用户/系统/开启/关闭/已变更）。
- 变更应用后可一键**重启新增/减的定义域**（force-stop 批量生效）。
- **双主题**：MIUIX 与 Material，跟随系统深色模式。
- 关于页支持**检查 GitHub 更新**并跳转下载。
- 应用图标解析：位图资源直读 + 自适应/矢量图标合成，无需联网。

### 功能
- 解除安装限制、恢复通知内容、自定义学习 / 应用使用时长。
- 屏蔽环境检测与 Root 痕迹、禁用未成年人精细化管控。

### 说明
- 适配学而思学习机 TALIH-PD2（Android 13 / arm64）。
- 需配合 ZygiskNext 使用。
