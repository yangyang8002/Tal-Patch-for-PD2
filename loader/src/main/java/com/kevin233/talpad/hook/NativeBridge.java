package com.kevin233.talpad.hook;

import java.lang.reflect.Member;
import java.lang.reflect.Method;

/**
 * Zygisk native 层（libtalpatch.so）的 JNI 接口声明。
 * native 实现位于 zygisk/jni/bridge.cpp，基于 LSPlant 完成
 * 方法替换、原方法备份与反优化。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class NativeBridge {
    private NativeBridge() {
    }

    /**
     * Hook 指定方法。
     *
     * @param hookId   HookBridge 分配的 id，存入 HookerStub 供回调定位
     * @param target   目标方法
     * @param isStatic 目标方法是否为 static（LSPlant 回调的 args 布局不同）
     * @return 原方法备份（可反射调用），失败返回 null
     */
    public static native Method nativeHook(long hookId, Method target,
                                           boolean isStatic);

    /** 反优化指定方法（消除内联导致的 Hook 失效，尽力而为）。 */
    public static native void nativeDeoptimize(Member target);

    /**
     * native 自检：返回 LSPlant 初始化是否成功。
     * 注入早期调用，失败则 Java 侧不再安装任何 Hook。
     */
    public static native boolean nativeReady();
}
