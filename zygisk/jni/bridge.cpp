/*
 * NativeBridge JNI 实现：基于 LSPlant 完成 Java 方法 Hook。
 *
 * LSPlant 回调约定（v6.x）：Hook(target, hooker, callback) 后，
 * 目标方法被调用时转入 callback(hooker, originalMethod, thisObject, args)；
 * 返回的 ScopedBackup 为原方法备份（java.lang.reflect.Method），反射调用即可执行原实现。
 * >>> 若升级 LSPlant 版本，仅需按 lsplant.hpp 调整本文件 <<<
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
#include <jni.h>

#include "lsplant.hpp"
#include "dobby.h"

#include "log.h"

namespace talpatch {

static bool g_ready = false;
static jmethodID g_callback;   // NativeBridge.lsplantCallback
static jclass g_long_cls;
static jmethodID g_long_value_of;

static bool init_lsplant(JNIEnv *env) {
    if (g_ready) return true;
    // InitInfo 字段名以 LSPlant v6.5 头文件为准
    lsplant::InitInfo info{
            .inline_hooker = [](void *target, void *replace, void **backup) {
                return DobbyHook(target, (dobby_dummy_func_t) replace,
                                 (dobby_dummy_func_t *) backup) == 0;
            },
            .inline_dehooker = [](void *target) {
                return DobbyDestroy(target) == 0;
            },
    };
    g_ready = lsplant::Init(env, info);
    LOGI("lsplant init: %d", g_ready);
    return g_ready;
}

// boolean NativeBridge.nativeReady()
static jboolean native_ready(JNIEnv *, jclass) {
    return g_ready ? JNI_TRUE : JNI_FALSE;
}

// Method NativeBridge.nativeHook(long id, Method target)
static jobject native_hook(JNIEnv *env, jclass, jlong id, jobject target) {
    if (!g_ready || target == nullptr || g_callback == nullptr) {
        return nullptr;
    }
    jobject hooker = env->CallStaticObjectMethod(g_long_cls, g_long_value_of, id);
    jobject hooker_ref = env->NewGlobalRef(hooker);
    env->DeleteLocalRef(hooker);
    auto backup = lsplant::Hook(env, target, hooker_ref, g_callback);
    if (!backup) {
        LOGW("lsplant hook failed for %p", target);
        return nullptr;
    }
    // 备份方法需长期持有（Java 侧 Chain.proceed 反射调用）
    return env->NewGlobalRef((jobject) backup);
}

// void NativeBridge.nativeDeoptimize(Member m)
static void native_deoptimize(JNIEnv *env, jclass, jobject member) {
    if (!g_ready || member == nullptr) return;
    lsplant::DeOptimize(env, member); // 尽力而为；如签名变化请按 lsplant.hpp 调整
}

static JNINativeMethod g_methods[] = {
        {"nativeHook",       "(JLjava/lang/reflect/Method;)Ljava/lang/reflect/Method;",
                (void *) native_hook},
        {"nativeDeoptimize", "(Ljava/lang/reflect/Member;)V",
                (void *) native_deoptimize},
        {"nativeReady",      "()Z",
                (void *) native_ready},
};

// inject.cpp 调用：注册 natives 并初始化 LSPlant
bool register_native_bridge(JNIEnv *env, jclass bridge_class) {
    if (!init_lsplant(env)) {
        return false;
    }
    if (env->RegisterNatives(bridge_class, g_methods,
                             sizeof(g_methods) / sizeof(g_methods[0])) != JNI_OK) {
        LOGE("RegisterNatives failed");
        env->ExceptionClear();
        return false;
    }
    g_callback = env->GetStaticMethodID(
            bridge_class, "lsplantCallback",
            "(Ljava/lang/Object;Ljava/lang/reflect/Method;"
            "Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
    if (g_callback == nullptr) {
        LOGE("lsplantCallback not found");
        env->ExceptionClear();
        return false;
    }
    g_long_cls = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Long"));
    g_long_value_of = env->GetStaticMethodID(g_long_cls, "valueOf",
                                             "(J)Ljava/lang/Long;");
    return true;
}

} // namespace talpatch
