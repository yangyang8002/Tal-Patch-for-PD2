package com.kevin233.talpad.tools;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.os.Looper;
import android.util.Base64;
import android.util.TypedValue;

import org.json.JSONArray;
import org.json.JSONObject;
import org.xmlpull.v1.XmlPullParser;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
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
 * 图标加载说明：裸 app_process 进程里 Resources.getDrawable 的 ImageDecoder
 * 路径不可用（全部 NotFoundException），因此直接走
 * getValue → openNonAsset → BitmapFactory 的可靠管线；
 * XML 形式的自适应图标则解析 AXML 取 background/foreground 位图合成。
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

    private static Context sSysCtx;
    private static Method sOpenNonAsset;
    private static Method sOpenXml;

    private PkgInfo() {
    }

    public static void main(String[] args) {
        String outPath = args.length > 0 ? args[0]
                : "/data/adb/modules/tal_patch/cache/apps.json";
        try {
            String json = dump();
            File out = new File(outPath);
            File parent = out.getParentFile();
            if (parent != null) parent.mkdirs();
            FileOutputStream fos = new FileOutputStream(out);
            fos.write(json.getBytes("UTF-8"));
            fos.close();
            out.setReadable(true, false);
            System.out.println("OK " + outPath + " (" + json.length() + " bytes)");
        } catch (Throwable t) {
            System.err.println("PkgInfo failed: " + t);
            t.printStackTrace();
            System.exit(1);
        }
    }

    private static String dump() throws Throwable {
        Context ctx = systemContext();
        sSysCtx = ctx;
        PackageManager pm = ctx.getPackageManager();
        List<ApplicationInfo> apps = pm.getInstalledApplications(0);
        Collections.sort(apps, new Comparator<ApplicationInfo>() {
            @Override
            public int compare(ApplicationInfo a, ApplicationInfo b) {
                return a.packageName.compareTo(b.packageName);
            }
        });
        JSONArray arr = new JSONArray();
        int iconOk = 0;
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
            Bitmap icon = loadIconBitmap(ai);
            if (icon != null) {
                o.put("icon", "data:image/png;base64," + bitmapToBase64(icon));
                icon.recycle();
                iconOk++;
            } else {
                o.put("icon", JSONObject.NULL);
            }
            arr.put(o);
        }
        System.out.println("icons: " + iconOk + "/" + apps.size());
        return arr.toString();
    }

    private static Context systemContext() throws Throwable {
        // 隐藏 API 反射：app_process 未启用 hidden API enforcement，可直接反射
        // 注意必须先准备主 Looper，否则 systemMain 会崩溃
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

    // ---------------- 图标加载（绕过 Resources.getDrawable） ----------------

    private static Bitmap loadIconBitmap(ApplicationInfo ai) {
        if (ai.icon == 0) return null;
        try {
            Context appCtx = sSysCtx.createPackageContext(ai.packageName, 0);
            Resources res = appCtx.getResources();
            return loadDrawableBitmap(res, ai.icon, 0);
        } catch (Throwable t) {
            return null;
        }
    }

    /**
     * 资源 id → Bitmap。resolveRefs 后按类型分派：
     * 位图文件直接解码；XML（自适应/矢量图标）解析 AXML 合成。
     *
     * @param depth 递归深度（XML 引用链）
     */
    private static Bitmap loadDrawableBitmap(Resources res, int resId, int depth) {
        if (resId == 0 || depth > 2) return null;
        try {
            TypedValue tv = new TypedValue();
            res.getValue(resId, tv, true);
            if (tv.string == null) return null;
            String path = tv.string.toString();
            if (path.endsWith(".xml")) {
                return loadXmlIcon(res, tv.assetCookie, path, depth + 1);
            }
            InputStream is = openNonAsset(res.getAssets(), tv.assetCookie, path);
            if (is == null) return null;
            Bitmap bmp = BitmapFactory.decodeStream(is);
            is.close();
            return bmp;
        } catch (Throwable t) {
            return null;
        }
    }

    /** 解析自适应图标等 XML，取 background/foreground 位图按自适应规范合成。 */
    private static Bitmap loadXmlIcon(Resources res, int cookie, String path, int depth) {
        try {
            XmlResourceParser parser = openXml(res.getAssets(), cookie, path);
            if (parser == null) return null;
            Bitmap bg = null;
            Bitmap fg = null;
            int event = parser.getEventType();
            while (event != XmlPullParser.END_DOCUMENT) {
                if (event == XmlPullParser.START_TAG) {
                    String name = parser.getName();
                    if ("background".equals(name) || "foreground".equals(name)) {
                        for (int i = 0; i < parser.getAttributeCount(); i++) {
                            String attr = parser.getAttributeName(i);
                            if (!"drawable".equals(attr)) continue;
                            int ref = parser.getAttributeResourceValue(i, 0);
                            if (ref == 0) continue;
                            Bitmap layer = loadDrawableBitmap(res, ref, depth);
                            if (layer != null) {
                                if ("background".equals(name)) bg = layer; else fg = layer;
                            }
                        }
                    }
                }
                event = parser.next();
            }
            parser.close();
            if (bg == null && fg == null) return null;
            // 自适应图标：内容层全幅绘制（WebUI 负责圆角裁剪）
            Bitmap out = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(out);
            if (bg != null) {
                canvas.drawBitmap(Bitmap.createScaledBitmap(bg, ICON_SIZE, ICON_SIZE, true),
                        0, 0, null);
                bg.recycle();
            } else {
                canvas.drawColor(0xFFF0F0F0);
            }
            if (fg != null) {
                canvas.drawBitmap(Bitmap.createScaledBitmap(fg, ICON_SIZE, ICON_SIZE, true),
                        0, 0, null);
                fg.recycle();
            }
            return out;
        } catch (Throwable t) {
            return null;
        }
    }

    private static InputStream openNonAsset(AssetManager am, int cookie, String path) {
        try {
            if (sOpenNonAsset == null) {
                sOpenNonAsset = AssetManager.class.getDeclaredMethod(
                        "openNonAsset", int.class, String.class, int.class);
                sOpenNonAsset.setAccessible(true);
            }
            return (InputStream) sOpenNonAsset.invoke(am, cookie, path, 1 /* ACCESS_STREAMING */);
        } catch (Throwable t) {
            return null;
        }
    }

    private static XmlResourceParser openXml(AssetManager am, int cookie, String path) {
        try {
            if (sOpenXml == null) {
                sOpenXml = AssetManager.class.getDeclaredMethod(
                        "openXmlResourceParser", int.class, String.class);
                sOpenXml.setAccessible(true);
            }
            return (XmlResourceParser) sOpenXml.invoke(am, cookie, path);
        } catch (Throwable t) {
            return null;
        }
    }

    private static String bitmapToBase64(Bitmap bmp) {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bmp.compress(Bitmap.CompressFormat.PNG, 90, bos);
        return Base64.encodeToString(bos.toByteArray(), Base64.NO_WRAP);
    }
}
