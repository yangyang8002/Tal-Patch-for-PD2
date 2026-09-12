# TAL-Patch Zygisk 模块 native 构建
# 依赖（首次构建时自动拉取）：
#   - zygisk-module-sample : Zygisk API 头文件 api.hpp
#   - LSPlant              : ART Java 方法 hook（Java 层 HookBridge 的底层实现）
#   - Dobby                : native inline hook（/proc/self/maps 过滤等）
cmake_minimum_required(VERSION 3.22.1)
project(talpatch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---- Zygisk API header (api.hpp) ----
FetchContent_Declare(zygisk_api
        GIT_REPOSITORY https://github.com/topjohnwu/zygisk-module-sample.git
        GIT_TAG master)
FetchContent_MakeAvailable(zygisk_api)
file(GLOB_RECURSE ZYGISK_API_HPP "${zygisk_api_SOURCE_DIR}/*api.hpp")
if (NOT ZYGISK_API_HPP)
    message(FATAL_ERROR "api.hpp not found in zygisk-module-sample")
endif ()
list(GET ZYGISK_API_HPP 0 ZYGISK_API_HPP)
get_filename_component(ZYGISK_API_DIR "${ZYGISK_API_HPP}" DIRECTORY)
message(STATUS "Zygisk api.hpp: ${ZYGISK_API_HPP}")

# ---- LSPlant ----
set(LSPLANT_STANDALONE ON CACHE BOOL "" FORCE)
FetchContent_Declare(lsplant
        GIT_REPOSITORY https://github.com/LSPosed/LSPlant.git
        GIT_TAG v6.5)
FetchContent_MakeAvailable(lsplant)

# ---- Dobby ----
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(dobby
        GIT_REPOSITORY https://github.com/jmpews/Dobby.git
        GIT_TAG master)
FetchContent_MakeAvailable(dobby)

add_library(talpatch SHARED
        jni/module.cpp
        jni/config.cpp
        jni/inject.cpp
        jni/bridge.cpp
        jni/native_hooks.cpp)

target_include_directories(talpatch PRIVATE "${ZYGISK_API_DIR}")
target_compile_options(talpatch PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti)
target_link_options(talpatch PRIVATE -Wl,--exclude-libs,ALL)

# 若 LSPlant/Dobby 的目标名与上游不一致，请按其实际 CMake target 调整
target_link_libraries(talpatch PRIVATE lsplant dobby log)
