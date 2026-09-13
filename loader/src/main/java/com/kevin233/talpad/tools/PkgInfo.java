package com.kevin233.talpad.tools;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Looper;
import android.util.Base64;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * 包信息导出工具（经 app_process 以 root 身份独立运行，不参与 hook）。
 *
 * 输出本机全部应用的 包名/应用名/图标(PNG base64)/是否系统应用 的 JSON，
 * 供 KernelSU WebUI 的「消息通知（应用粒度）」列表展示。
 *
 * 用法：
 *   app_process -Djava.class.path=/data/adb/modules/tal_patch/loader.dex \
 *       /system/bin com.kevin233.talpad.tools.PkgInfo /path/to/apps.json
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class PkgInfo {
    private static final int ICON_SIZE = 96; // px，PNG 输出尺寸

    private PkgInfo() {
    }

    public static void main(String[] args) {
        String outPath = args.length > 0 ? args[0]
                : "/data/adb/modules/tal_patch/cache/apps.json";
        try {
            String json = dump(outPath);
            File out = new File(outPath);
            File parent = out.getParentFile();
            if (parent != null) parent.mkdirs();
            FileOutputStream fos = new FileOutputStream(out);
            fos.write(json.getBytes("UTF-8"));
            fos.close();
            // WebUI 以 root 读取，0644 即可
            out.setReadable(true, false);
            System.out.println("OK " + outPath + " (" + json.length() + " bytes)");
        } catch (Throwable t) {
            System.err.println("PkgInfo failed: " + t);
            t.printStackTrace();
            System.exit(1);
        }
    }

    private static String dump(String outPath) throws Throwable {
        // app_process 裸进程没有 Context，借 ActivityThread 的系统上下文拿 PackageManager
        Context ctx = systemContext();
        PackageManager pm = ctx.getPackageManager();
        List<ApplicationInfo> apps = pm.getInstalledApplications(0);
        Collections.sort(apps, new Comparator<ApplicationInfo>() {
            @Override
            public int compare(ApplicationInfo a, ApplicationInfo b) {
                return a.packageName.compareTo(b.packageName);
            }
        });
        JSONArray arr = new JSONArray();
        for (ApplicationInfo ai : apps) {
            JSONObject o = new JSONObject();
            o.put("pkg", ai.packageName);
            boolean sys = (ai.flags & ApplicationInfo.FLAG_SYSTEM) != 0
                    || (ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0;
            o.put("system", sys);
            CharSequence label;
            try {
                label = pm.getApplicationLabel(ai);
            } catch (Throwable t) {
                label = ai.packageName;
            }
            o.put("label", label == null ? ai.packageName : label.toString());
            try {
                Drawable d = loadIconRaw(ai);
                if (d == null) {
                    o.put("icon", JSONObject.NULL);
                } else {
                    o.put("icon", "data:image/png;base64," + iconToBase64(d));
                }
            } catch (Throwable t) {
                o.put("icon", JSONObject.NULL);
            }
            arr.put(o);
        }
        return arr.toString();
    }

    private static Context systemContext() throws Throwable {
        // 隐藏 API 反射：app_process 未启用 hidden API enforcement，可直接反射
        try {
            Looper.prepareMainLooper();
        } catch (Throwable ignored) {
        }
        Class<?> atClass = Class.forName("android.app.ActivityThread");
        Method systemMain = atClass.getDeclaredMethod("systemMain");
        systemMain.setAccessible(true);
        Object at = systemMain.invoke(null);
        Method getSystemContext = atClass.getDeclaredMethod("getSystemContext");
        getSystemContext.setAccessible(true);
        return (Context) getSystemContext.invoke(at);
    }

    /**
     * 手动加载应用图标：app_process 裸进程里系统上下文的 DisplayMetrics
     * 不完整（densityDpi 可能为 0），PackageManager.getApplicationIcon
     * 内部选资源失败会全部回退成系统默认图标。这里直接给目标 APK 建
     * AssetManager + 显式 xxhdpi 指标，绕过该问题。
     */
    private static Drawable loadIconRaw(ApplicationInfo ai) {
        if (ai.icon == 0) return null;
        try {
            AssetManager am = AssetManager.class.newInstance();
            Method add = AssetManager.class.getMethod(
                    "addAssetPath", String.class);
            add.setAccessible(true);
            add.invoke(am, ai.sourceDir);
            if (ai.splitSourceDirs != null) {
                for (String sp : ai.splitSourceDirs) {
                    add.invoke(am, sp);
                }
            }
            DisplayMetrics dm = new DisplayMetrics();
            dm.densityDpi = 480;
            dm.density = 3.0f;
            dm.scaledDensity = 3.0f;
            dm.xdpi = 480;
            dm.ydpi = 480;
            dm.widthPixels = 1080;
            dm.heightPixels = 1920;
            Resources res = new Resources(am, dm, new Configuration());
            return res.getDrawable(ai.icon, null);
        } catch (Throwable t) {
            return null;
        }
    }

    private static String iconToBase64(Drawable d) throws Throwable {
        Bitmap bmp = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bmp);
        d.setBounds(0, 0, ICON_SIZE, ICON_SIZE);
        d.draw(canvas);
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bmp.compress(Bitmap.CompressFormat.PNG, 90, bos);
        bmp.recycle();
        return Base64.encodeToString(bos.toByteArray(), Base64.NO_WRAP);
    }
}
