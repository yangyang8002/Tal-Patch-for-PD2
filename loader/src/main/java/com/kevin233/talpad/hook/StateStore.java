package com.kevin233.talpad.hook;

import android.os.Environment;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

/**
 * 跨进程共享的小文件状态存储（每日伪装标记等），
 * 取代旧版基于 LSPosed openRemoteFile 的实现。
 *
 * 目录：/sdcard/TAL-Patch/state/（目标进程均可读写，
 * 不可用时退化为当前应用私有外部目录）。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class StateStore {
    private StateStore() {
    }

    public static String read(String name) {
        byte[] data = readBytes(name);
        if (data == null) {
            return null;
        }
        return new String(data, StandardCharsets.UTF_8).trim();
    }

    public static byte[] readBytes(String name) {
        File file = resolve(name);
        if (file == null || !file.isFile()) {
            return null;
        }
        try {
            FileInputStream in = new FileInputStream(file);
            byte[] buf = new byte[(int) Math.min(file.length(), 1 << 20)];
            int n = in.read(buf);
            in.close();
            if (n <= 0) {
                return null;
            }
            byte[] out = new byte[n];
            System.arraycopy(buf, 0, out, 0, n);
            return out;
        } catch (Throwable t) {
            return null;
        }
    }

    public static void write(String name, String content) {
        write(name, content == null ? new byte[0]
                : content.getBytes(StandardCharsets.UTF_8));
    }

    public static void write(String name, byte[] content) {
        File file = resolve(name);
        if (file == null) {
            return;
        }
        try {
            File parent = file.getParentFile();
            if (parent != null && !parent.isDirectory()) {
                parent.mkdirs();
            }
            FileOutputStream out = new FileOutputStream(file, false);
            out.write(content);
            out.flush();
            out.close();
            try {
                file.setReadable(true, false);
            } catch (Throwable ignored) {
            }
        } catch (Throwable ignored) {
        }
    }

    private static File resolve(String name) {
        try {
            File dir = new File(Environment.getExternalStorageDirectory(),
                    "TAL-Patch/state");
            if (dir.isDirectory() || dir.mkdirs()) {
                return new File(dir, name);
            }
        } catch (Throwable ignored) {
        }
        try {
            android.content.Context ctx = appContext();
            if (ctx != null) {
                File dir = ctx.getExternalFilesDir(null);
                if (dir != null) {
                    return new File(dir, name);
                }
            }
        } catch (Throwable ignored) {
        }
        return null;
    }

    private static android.content.Context appContext() {
        try {
            Object app = Class.forName("android.app.ActivityThread")
                    .getMethod("currentApplication").invoke(null);
            return app instanceof android.content.Context
                    ? (android.content.Context) app : null;
        } catch (Throwable t) {
            return null;
        }
    }
}
