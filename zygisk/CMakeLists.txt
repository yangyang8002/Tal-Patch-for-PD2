# TAL-Patch Zygisk 模块 native 构建
#
# 依赖（构建时自动拉取）：
#   - topjohnwu/zygisk-module-sample : Zygisk API 头文件 zygisk.hpp
#   - LSPosed/LSPlant v6.4           : ART Java 方法 hook（含子模块 DexBuilder/parallel-hashmap）
#   - jmpews/Dobby master            : native inline hook + libart 符号解析
#
# 注意：DexBuilder 要求 CMake >= 3.28；Dobby 与 CMake 4.x 不兼容。
#       请使用 CMake 3.28 ~ 3.31（CI 中为 3.31.x）。
cmake_minimum_required(VERSION 3.28)
project(talpatch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
find_package(Git REQUIRED)

# ---- Zygisk API header (zygisk.hpp，该仓库是 gradle 工程，无 CMakeLists) ----
FetchContent_Populate(zygisk_api
        GIT_REPOSITORY https://github.com/topjohnwu/zygisk-module-sample.git
        GIT_TAG master)
set(ZYGISK_API_DIR "${zygisk_api_SOURCE_DIR}/module/jni")
if (NOT EXISTS "${ZYGISK_API_DIR}/zygisk.hpp")
    message(FATAL_ERROR "zygisk.hpp not found: ${ZYGISK_API_DIR}")
endif ()

# ---- Dobby ----
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
FetchContent_Declare(dobby
        GIT_REPOSITORY https://github.com/jmpews/Dobby.git
        GIT_TAG master)
FetchContent_MakeAvailable(dobby)

# ---- LSPlant（仅拉取构建所需子模块，避开 git@ 协议的 test/docs 子模块） ----
set(LSPLANT_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Populate(lsplant
        GIT_REPOSITORY https://github.com/LSPosed/LSPlant.git
        GIT_TAG v6.4
        GIT_SUBMODULES "")
execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                -- lsplant/src/main/jni/external/dex_builder
        WORKING_DIRECTORY "${lsplant_SOURCE_DIR}"
        RESULT_VARIABLE LSPLANT_SUBMODULE_RESULT)
if (NOT LSPLANT_SUBMODULE_RESULT EQUAL 0)
    message(FATAL_ERROR "LSPlant dex_builder submodule fetch failed")
endif ()
add_subdirectory("${lsplant_SOURCE_DIR}/lsplant/src/main/jni" "${lsplant_BINARY_DIR}")

add_library(talpatch SHARED
        jni/module.cpp
        jni/config.cpp
        jni/inject.cpp
        jni/bridge.cpp
        jni/native_hooks.cpp)

target_include_directories(talpatch PRIVATE "${ZYGISK_API_DIR}")
target_compile_options(talpatch PRIVATE -fvisibility=hidden -fno-rtti)
target_link_options(talpatch PRIVATE -Wl,--exclude-libs,ALL)

target_link_libraries(talpatch PRIVATE lsplant_static dobby_static log)
