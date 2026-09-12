package com.kevin233.talpad.hook;

import android.util.Log;

import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Java 层 Hook 桥：对外提供与旧 libxposed 类似的链式 API
 *（Chain.proceed / getArg / getThisObject），底层经 {@link NativeBridge}
 * 交给 Zygisk 注入的 native 代码（LSPlant）完成 ART 方法替换。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class HookBridge {
    private static final String TAG = "TAL-Patch";

    /** Hook 回调，等价于旧 XposedInterface.Hooker。 */
    public interface Hooker {
        Object intercept(Chain chain) throws Throwable;
    }

    /** 一次调用的上下文。proceed() 通过 backup 方法调用原实现。 */
    public static final class Chain {
        private final Entry entry;
        private final Object thisObject;
        private final Object[] args;

        Chain(Entry entry, Object thisObject, Object[] args) {
            this.entry = entry;
            this.thisObject = thisObject;
            this.args = args == null ? new Object[0] : args;
        }

        public Object getThisObject() {
            return thisObject;
        }

        public Object getArg(int index) {
            return index >= 0 && index < args.length ? args[index] : null;
        }

        public Object proceed() throws Throwable {
            return proceed(args);
        }

        public Object proceed(Object[] newArgs) throws Throwable {
            Method backup = entry.backup;
            if (backup == null) {
                throw new IllegalStateException(
                        "no backup for " + entry.target);
            }
            try {
                return backup.invoke(thisObject,
                        newArgs == null ? args : newArgs);
            } catch (java.lang.reflect.InvocationTargetException e) {
                Throwable cause = e.getCause();
                throw cause == null ? e : cause;
            }
        }
    }

    static final class Entry {
        final long id;
        final Member target;
        final Hooker hooker;
        volatile Method backup;

        Entry(long id, Member target, Hooker hooker) {
            this.id = id;
            this.target = target;
            this.hooker = hooker;
        }
    }

    private static final AtomicLong sNextId = new AtomicLong(1);
    private static final Map<Long, Entry> sEntries = new ConcurrentHashMap<>();
    private static final Map<Member, Entry> sByTarget = new ConcurrentHashMap<>();

    private HookBridge() {
    }

    /** 安装 Hook。返回是否成功（native 层失败时返回 false）。 */
    public static synchronized boolean hook(Member target, Hooker hooker) {
        if (target == null || hooker == null) {
            return false;
        }
        Entry existing = sByTarget.get(target);
        if (existing != null) {
            return true;
        }
        if (!(target instanceof Method)) {
            Log.w(TAG, "only Method hooking supported: " + target);
            return false;
        }
        try {
            ((Method) target).setAccessible(true);
        } catch (Throwable ignored) {
        }
        long id = sNextId.getAndIncrement();
        Entry entry = new Entry(id, target, hooker);
        Method backup;
        try {
            backup = NativeBridge.nativeHook(id, (Method) target);
        } catch (Throwable t) {
            Log.w(TAG, "nativeHook failed for " + target, t);
            return false;
        }
        if (backup == null) {
            Log.w(TAG, "nativeHook returned null backup for " + target);
            return false;
        }
        entry.backup = backup;
        try {
            backup.setAccessible(true);
        } catch (Throwable ignored) {
        }
        sEntries.put(id, entry);
        sByTarget.put(target, entry);
        return true;
    }

    /** 反优化，保证 Hook 稳定（可被 native 层否决）。 */
    public static void deoptimize(Member target) {
        try {
            NativeBridge.nativeDeoptimize(target);
        } catch (Throwable ignored) {
        }
    }

    /**
     * 调用方法的原始实现（绕过 Hook）。未 Hook 的方法退化为普通反射调用。
     * 取代旧版 XposedModule#getInvoker(Type.ORIGIN)。
     */
    public static Object invokeOriginal(Method method, Object thisObject,
                                        Object... args) throws Throwable {
        Entry entry = sByTarget.get(method);
        if (entry != null && entry.backup != null) {
            try {
                return entry.backup.invoke(thisObject, args);
            } catch (java.lang.reflect.InvocationTargetException e) {
                Throwable cause = e.getCause();
                throw cause == null ? e : cause;
            }
        }
        try {
            method.setAccessible(true);
        } catch (Throwable ignored) {
        }
        try {
            return method.invoke(thisObject, args);
        } catch (java.lang.reflect.InvocationTargetException e) {
            Throwable cause = e.getCause();
            throw cause == null ? e : cause;
        }
    }

    /**
     * native trampoline 回调入口（由 LSPlant 替换后的方法转入）。
     * 保护模式：Hooker 抛异常时记录并执行原方法。
     */
    @SuppressWarnings("unused") // called from native
    public static Object dispatch(long id, Object thisObject, Object[] args)
            throws Throwable {
        Entry entry = sEntries.get(id);
        if (entry == null) {
            return null;
        }
        Chain chain = new Chain(entry, thisObject, args);
        try {
            return entry.hooker.intercept(chain);
        } catch (Throwable t) {
            Log.w(TAG, "hooker threw for " + entry.target
                    + ", proceeding original", t);
            return chain.proceed();
        }
    }
}
