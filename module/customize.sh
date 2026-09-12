#!/system/bin/sh
# TAL-Patch (Zygisk + WebUI) 安装脚本
# Author: Kevin233 & yangyang8002

SKIPUNZIP=0

ui_print "********************************"
ui_print " TAL-Patch (Zygisk + WebUI)"
ui_print " Kevin233 & yangyang8002"
ui_print "********************************"

# ABI 检查
if [ "$ARCH" != "arm64" ] && [ "$ARCH" != "arm" ]; then
  ui_print "! 不支持的架构: $ARCH"
  abort
fi

# 解包 zygisk so / loader.dex / webroot（模块包内路径直接对应 MODPATH）
unzip -o "$ZIPFILE" 'zygisk/*' -d "$MODPATH" >&2
unzip -o "$ZIPFILE" 'loader.dex' -d "$MODPATH" >&2
unzip -o "$ZIPFILE" 'webroot/*' -d "$MODPATH" >&2
unzip -o "$ZIPFILE" 'service.sh' -d "$MODPATH" >&2

# 升级安装时保留旧配置；首次安装写入默认配置
mkdir -p "$MODPATH/config"
if [ -f /data/adb/modules/tal_patch/config/config.json ]; then
  ui_print "- 保留已有配置"
  cp -af /data/adb/modules/tal_patch/config/config.json "$MODPATH/config/config.json"
elif [ ! -f "$MODPATH/config/config.json" ]; then
  ui_print "- 写入默认配置"
  unzip -o "$ZIPFILE" 'config.default.json' -d "$MODPATH" >&2
  cp -af "$MODPATH/config.default.json" "$MODPATH/config/config.json"
fi
rm -f "$MODPATH/config.default.json"

# 同步镜像配置到 /sdcard（供已注入进程热更新读取）
mkdir -p /sdcard/TAL-Patch/state
cp -af "$MODPATH/config/config.json" /sdcard/TAL-Patch/config.json 2>/dev/null
chmod 0666 /sdcard/TAL-Patch/config.json 2>/dev/null

set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/loader.dex" 0 0 0644
set_perm "$MODPATH/config/config.json" 0 0 0644

ui_print "- 安装完成，重启后生效"
ui_print "- 配置入口：KernelSU 管理器 → 模块 → TAL-Patch → WebUI"
