#!/system/bin/sh
# TAL-Patch late_start service：开机后同步一次配置镜像
MODDIR=${0%/*}

while [ "$(getprop sys.boot_completed)" != "1" ]; do
  sleep 2
done
sleep 5

mkdir -p /sdcard/TAL-Patch/state
if [ -f "$MODDIR/config/config.json" ]; then
  cp -af "$MODDIR/config/config.json" /sdcard/TAL-Patch/config.json 2>/dev/null
  chmod 0666 /sdcard/TAL-Patch/config.json 2>/dev/null
fi
