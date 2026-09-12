#pragma once

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

namespace talpatch {

// 在目标进程中：以 InMemoryDexClassLoader 内存加载 loader.dex，
// 注册 NativeBridge 的 JNI 方法，并调用 HookEntry.init(processName, configJson)。
// dex_bytes 必须为堆上持久副本（DirectByteBuffer 引用它）。
bool inject_loader(JNIEnv *env, const char *process_name,
                   const std::string &config_json,
                   std::vector<uint8_t> dex_bytes);

} // namespace talpatch
