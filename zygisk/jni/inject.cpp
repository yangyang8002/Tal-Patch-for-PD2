#include "inject.hpp"

#include <cstring>

#include "log.h"

namespace talpatch {

// bridge.cpp 提供：向已加载的 NativeBridge 类注册 JNI natives 并完成 LSPlant 初始化
bool register_native_bridge(JNIEnv *env, jclass bridge_class);

static jclass find_class(JNIEnv *env, jobject class_loader, const char *name) {
    // Class.forName(name, false, classLoader)
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

static jobject default_parent_class_loader(JNIEnv *env) {
    // Thread.currentThread().getContextClassLoader()
    jclass thread_cls = env->FindClass("java/lang/Thread");
    jmethodID current = env->GetStaticMethodID(
            thread_cls, "currentThread", "()Ljava/lang/Thread;");
    jobject thread = env->CallStaticObjectMethod(thread_cls, current);
    jmethodID get_cl = env->GetMethodID(
            thread_cls, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cl = env->CallObjectMethod(thread, get_cl);
    env->DeleteLocalRef(thread);
    env->DeleteLocalRef(thread_cls);
    if (cl == nullptr) {
        // system_server 早期兜底：ClassLoader.getSystemClassLoader()
        jclass cl_cls = env->FindClass("java/lang/ClassLoader");
        jmethodID sys = env->GetStaticMethodID(
                cl_cls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        cl = env->CallStaticObjectMethod(cl_cls, sys);
        env->DeleteLocalRef(cl_cls);
    }
    return cl;
}

bool inject_loader(JNIEnv *env, const char *process_name,
                   const std::string &config_json,
                   std::vector<uint8_t> dex_bytes) {
    if (dex_bytes.empty()) {
        LOGE("inject: loader.dex is empty, abort");
        return false;
    }

    // 直接内存，避免 dex 落盘痕迹
    void *dex_mem = malloc(dex_bytes.size());
    if (!dex_mem) {
        LOGE("inject: malloc failed");
        return false;
    }
    memcpy(dex_mem, dex_bytes.data(), dex_bytes.size());
    jobject dex_buf = env->NewDirectByteBuffer(dex_mem, dex_bytes.size());
    if (!dex_buf) {
        LOGE("inject: NewDirectByteBuffer failed");
        free(dex_mem);
        return false;
    }

    jobject parent = default_parent_class_loader(env);

    jclass inmem_cls = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    jmethodID inmem_ctor = env->GetMethodID(
            inmem_cls, "<init>",
            "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    jobject loader = env->NewObject(inmem_cls, inmem_ctor, dex_buf, parent);
    if (env->ExceptionCheck() || loader == nullptr) {
        env->ExceptionClear();
        LOGE("inject: create InMemoryDexClassLoader failed");
        free(dex_mem);
        return false;
    }
    jobject loader_ref = env->NewGlobalRef(loader); // 保持存活，防止 dex 被回收

    // 注册 NativeBridge JNI
    jclass bridge = find_class(env, loader,
                               "com.kevin233.talpad.hook.NativeBridge");
    if (bridge == nullptr) {
        LOGE("inject: NativeBridge class not found");
        return false;
    }
    if (!register_native_bridge(env, bridge)) {
        LOGE("inject: register NativeBridge natives failed");
        return false;
    }

    // 调用 HookEntry.init(processName, configJson)
    jclass entry = find_class(env, loader,
                              "com.kevin233.talpad.hook.HookEntry");
    if (entry == nullptr) {
        LOGE("inject: HookEntry class not found");
        return false;
    }
    jmethodID init = env->GetStaticMethodID(
            entry, "init", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (init == nullptr) {
        env->ExceptionClear();
        LOGE("inject: HookEntry.init not found");
        return false;
    }
    jstring jprocess = env->NewStringUTF(process_name);
    jstring jconfig = config_json.empty()
                      ? nullptr : env->NewStringUTF(config_json.c_str());
    env->CallStaticVoidMethod(entry, init, jprocess, jconfig);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        LOGE("inject: HookEntry.init threw");
        return false;
    }
    env->DeleteLocalRef(jprocess);
    if (jconfig) env->DeleteLocalRef(jconfig);
    LOGI("injected into %s (loader=%p)", process_name, loader_ref);
    return true;
}

} // namespace talpatch
