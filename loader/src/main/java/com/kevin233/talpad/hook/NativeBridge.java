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
     * @param hookId HookBridge 分配的 id，trampoline 回调时回传
     * @param target 目标方法
     * @return 原方法备份（可反射调用），失败返回 null
     */
    public static native Method nativeHook(long hookId, Method target);

    /** 反优化指定方法（尽力而为）。 */
    public static native void nativeDeoptimize(Member target);

    /**
     * native 自检：返回 LSPlant 初始化是否成功。
     * 注入早期调用，失败则 Java 侧不再安装任何 Hook。
     */
    public static native boolean nativeReady();

    /**
     * LSPlant 回调入口（签名与 LSPlant v6 Hook 的 callback 约定一致）：
     * 目标方法被调用时，由 LSPlant 转入本方法。
     *
     * @param hooker   Hook 时 native 传入的上下文对象（Long 型的 hookId）
     * @param original 原方法（未使用，备份由 HookBridge 登记）
     * @param thiz     调用者实例（静态方法为 null）
     * @param args     调用参数
     */
    @SuppressWarnings("unused") // invoked by LSPlant trampoline
    public static Object lsplantCallback(Object hooker,
                                         java.lang.reflect.Method original,
                                         Object thiz, Object[] args)
            throws Throwable {
        long hookId = hooker instanceof Long ? (Long) hooker : -1L;
        return HookBridge.dispatch(hookId, thiz, args);
    }
}
