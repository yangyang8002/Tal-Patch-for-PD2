package com.kevin233.talpad.hook;

/**
 * LSPlant Hook 上下文对象。
 *
 * LSPlant 的约定（v6.4）：
 * {@code Hook(target, hooker, callback)} 后，目标方法被调用时转入
 * hooker 实例上的 {@code public Object callback(Object[] args)}；
 * 非静态方法 args[0] 为 this，静态方法 args 直接是参数列表。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class HookerStub {
    private final long hookId;
    private final boolean isStatic;

    /** 由 native 层（bridge.cpp）通过反射构造。 */
    @SuppressWarnings("unused") // called from native
    public HookerStub(long hookId, boolean isStatic) {
        this.hookId = hookId;
        this.isStatic = isStatic;
    }

    /** LSPlant 回调：签名必须是 public Object callback(Object[] args)。 */
    @SuppressWarnings("unused") // invoked by LSPlant trampoline
    public Object callback(Object[] args) throws Throwable {
        Object thiz;
        Object[] realArgs;
        if (isStatic) {
            thiz = null;
            realArgs = args == null ? new Object[0] : args;
        } else {
            thiz = (args != null && args.length > 0) ? args[0] : null;
            if (args != null && args.length > 1) {
                realArgs = new Object[args.length - 1];
                System.arraycopy(args, 1, realArgs, 0, realArgs.length);
            } else {
                realArgs = new Object[0];
            }
        }
        return HookBridge.dispatch(hookId, thiz, realArgs);
    }
}
