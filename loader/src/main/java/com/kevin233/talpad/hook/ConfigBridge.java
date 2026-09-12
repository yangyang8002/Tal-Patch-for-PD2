package com.kevin233.talpad.hook;

import android.os.Environment;
import android.os.SystemClock;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * 配置桥：取代旧版 LSPosed RemotePreferences / XposedService。
 *
 * 配置来源（按优先级合并）：
 *  1. Zygisk 注入时由 native 层传入的 JSON（在进程仍持有 root 权限的
 *     specialize 窗口期读取 /data/adb/modules/tal_patch/config/config.json）。
 *  2. /sdcard/TAL-Patch/config.json 热更新镜像（WebUI 保存时同步写出，
 *     本类按 mtime 轮询，实现免重启生效）。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class ConfigBridge {
    private static final String TAG = "TAL-Patch";
    private static final long POLL_INTERVAL_MS = 4000L;
    private static volatile Prefs sPrefs = new Prefs(null);
    private static volatile JSONObject sJson = new JSONObject();
    private static volatile boolean sWatcherStarted;
    private static volatile long sMirrorMtime = -1L;

    private ConfigBridge() {
    }

    /** 由 HookEntry.init 调用，json 为 native 层读取的原始配置（可为 null）。 */
    public static synchronized void init(String json) {
        apply(json);
        startMirrorWatcher();
    }

    public static Prefs get() {
        return sPrefs;
    }

    public static JSONObject json() {
        return sJson;
    }

    static synchronized void apply(String json) {
        if (json == null || json.trim().isEmpty()) {
            return;
        }
        try {
            JSONObject obj = new JSONObject(json);
            Map<String, Object> map = new HashMap<>();
            Iterator<String> keys = obj.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                Object value = obj.opt(key);
                if (value != null && value != JSONObject.NULL) {
                    map.put(key, value);
                }
            }
            sJson = obj;
            sPrefs = new Prefs(map);
            Log.i(TAG, "config applied: " + map.size() + " keys");
        } catch (Throwable t) {
            Log.w(TAG, "config parse failed", t);
        }
    }

    /** 轮询 /sdcard 镜像，实现 WebUI 保存后的热更新。 */
    private static void startMirrorWatcher() {
        if (sWatcherStarted) {
            return;
        }
        sWatcherStarted = true;
        Thread thread = new Thread(() -> {
            while (!Thread.currentThread().isInterrupted()) {
                try {
                    File mirror = mirrorFile();
                    if (mirror != null && mirror.isFile()) {
                        long mtime = mirror.lastModified();
                        if (mtime != sMirrorMtime) {
                            sMirrorMtime = mtime;
                            FileInputStream in = new FileInputStream(mirror);
                            byte[] buf = new byte[(int) Math.min(
                                    mirror.length(), 1 << 20)];
                            int n = in.read(buf);
                            in.close();
                            if (n > 0) {
                                apply(new String(buf, 0, n,
                                        java.nio.charset.StandardCharsets.UTF_8));
                            }
                        }
                    }
                } catch (Throwable ignored) {
                }
                try {
                    Thread.sleep(POLL_INTERVAL_MS);
                } catch (InterruptedException e) {
                    return;
                }
            }
        }, "TAL-Patch-ConfigWatcher");
        thread.setDaemon(true);
        thread.start();
    }

    private static File mirrorFile() {
        try {
            return new File(Environment.getExternalStorageDirectory(),
                    "TAL-Patch/config.json");
        } catch (Throwable t) {
            return null;
        }
    }

    /** 简单耗时埋点（调试用）。 */
    static long uptime() {
        return SystemClock.elapsedRealtime();
    }
}
