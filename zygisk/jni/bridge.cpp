/*
 * NativeBridge JNI 实现：基于 LSPlant v6.4 完成 Java 方法 Hook。
 *
 * LSPlant v6.4 约定（见 lsplant.hpp）：
 *   - Init(InitInfo)：必须提供 inline_hooker（返回 backup 指针）、
 *     inline_unhooker、art_symbol_resolver；
 *   - Hook(env, target, hooker_object, callback_method) → 返回 backup
 *     Method（jobject），对其反射 invoke 即执行原方法；
 *   - callback 是 hooker_object 的实例方法，签名必须为
 *     public Object callback(Object[] args)；非静态方法 args[0] 为 this；
 *   - Deoptimize(env, method) 用于消除内联导致的 Hook 失效。
 *
 * 内联 hook 引擎与 libart 符号解析均由 Dobby 提供。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
#include <jni.h>

#include <dlfcn.h>
#include <string>

#include "lsplant.hpp"
#include "dobby.h"

#include "log.h"

namespace talpatch {

static bool g_ready = false;

// com.kevin233.talpad.hook.HookerStub
static jclass g_stub_cls = nullptr;          // global ref
static jmethodID g_stub_ctor = nullptr;      // <init>(JZ)V
static jobject g_stub_callback = nullptr;    // Method: callback([Ljava/lang/Object;)Ljava/lang/Object;

static bool init_lsplant(JNIEnv *env) {
    if (g_ready) return true;
    lsplant::InitInfo info{
            .inline_hooker = [](void *target, void *hooker) -> void * {
                dobby_dummy_func_t backup = nullptr;
                if (DobbyHook(target, (dobby_dummy_func_t) hooker, &backup) != 0) {
                    return nullptr;
                }
                return (void *) backup;
            },
            .inline_unhooker = [](void *target) -> bool {
                return DobbyDestroy(target) == 0;
            },
            .art_symbol_resolver = [](std::string_view symbol) -> void * {
                std::string name(symbol);
                // 先查已加载符号（libart.so 已在本进程）
                void *addr = dlsym(RTLD_DEFAULT, name.c_str());
                if (addr) return addr;
                // 再从磁盘 ELF 的 .symtab/.dynsym 解析（APEX 中的 libart.so 未裁剪 symtab）
                return DobbySymbolResolver(nullptr, name.c_str());
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

// Method NativeBridge.nativeHook(long id, Method target, boolean isStatic)
static jobject native_hook(JNIEnv *env, jclass, jlong id, jobject target,
                           jboolean is_static) {
    if (!g_ready || target == nullptr || g_stub_cls == nullptr) {
        return nullptr;
    }
    jobject stub = env->NewObject(g_stub_cls, g_stub_ctor, id,
                                  is_static == JNI_TRUE);
    if (stub == nullptr) {
        env->ExceptionClear();
        LOGW("create HookerStub failed");
        return nullptr;
    }
    // hooker 对象由 LSPlant 生成的 stub 类静态字段持有，无需额外保活
    jobject backup = lsplant::Hook(env, target, stub, g_stub_callback);
    if (backup == nullptr) {
        LOGW("lsplant hook failed for %p", target);
    }
    return backup; // 作为返回值传给 Java 层，局部引用依然有效
}

// void NativeBridge.nativeDeoptimize(Member m)
static void native_deoptimize(JNIEnv *env, jclass, jobject member) {
    if (!g_ready || member == nullptr) return;
    lsplant::Deoptimize(env, member);
}

static JNINativeMethod g_methods[] = {
        {"nativeHook",       "(JLjava/lang/reflect/Method;Z)Ljava/lang/reflect/Method;",
                (void *) native_hook},
        {"nativeDeoptimize", "(Ljava/lang/reflect/Member;)V",
                (void *) native_deoptimize},
        {"nativeReady",      "()Z",
                (void *) native_ready},
};

static jclass find_with_loader(JNIEnv *env, jobject class_loader, const char *name) {
    jclass cls_class = env->FindClass("java/lang/Class");
    jmethodID for_name = env->GetStaticMethodID(
            cls_class, "forName",
            "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
    jstring jname = env->NewStringUTF(name);
    jobject result = env->CallStaticObjectMethod(
            cls_class, for_name, jname, JNI_FALSE, class_loader);
    env->DeleteLocalRef(jname);
    env->DeleteLocalRef(cls_class);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return (jclass) result;
}

// inject.cpp 调用：注册 natives、缓存 HookerStub 引用并初始化 LSPlant
bool register_native_bridge(JNIEnv *env, jclass bridge_class, jobject class_loader) {
    if (!init_lsplant(env)) {
        return false;
    }
    if (env->RegisterNatives(bridge_class, g_methods,
                             sizeof(g_methods) / sizeof(g_methods[0])) != JNI_OK) {
        LOGE("RegisterNatives failed");
        env->ExceptionClear();
        return false;
    }
    jclass stub = find_with_loader(env, class_loader,
                                   "com.kevin233.talpad.hook.HookerStub");
    if (stub == nullptr) {
        LOGE("HookerStub class not found");
        return false;
    }
    g_stub_cls = (jclass) env->NewGlobalRef(stub);
    g_stub_ctor = env->GetMethodID(g_stub_cls, "<init>", "(JZ)V");
    jmethodID cb = env->GetMethodID(g_stub_cls, "callback",
                                    "([Ljava/lang/Object;)Ljava/lang/Object;");
    if (g_stub_ctor == nullptr || cb == nullptr) {
        LOGE("HookerStub members not found");
        env->ExceptionClear();
        return false;
    }
    // LSPlant Hook 的 callback 参数需要 java.lang.reflect.Method 对象
    jclass class_cls = env->FindClass("java/lang/Class");
    jmethodID get_declared = env->GetMethodID(
            class_cls, "getDeclaredMethod",
            "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;");
    jstring cb_name = env->NewStringUTF("callback");
    jclass arr_cls = env->FindClass("[Ljava/lang/Object;");
    jobjectArray param_types = env->NewObjectArray(1, class_cls, arr_cls);
    g_stub_callback = env->NewGlobalRef(env->CallObjectMethod(
            stub, get_declared, cb_name, param_types));
    env->DeleteLocalRef(cb_name);
    env->DeleteLocalRef(param_types);
    env->DeleteLocalRef(class_cls);
    env->DeleteLocalRef(arr_cls);
    if (g_stub_callback == nullptr || env->ExceptionCheck()) {
        LOGE("HookerStub.callback Method object resolve failed");
        env->ExceptionClear();
        return false;
    }
    LOGI("native bridge registered");
    return true;
}

} // namespace talpatch
