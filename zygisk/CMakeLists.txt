# TAL-Patch Zygisk 模块 native 构建
#
# 依赖（构建时自动拉取到 build 目录，浅克隆）：
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

find_package(Git REQUIRED)

set(DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")
file(MAKE_DIRECTORY "${DEPS_DIR}")

# 浅克隆辅助：deps_clone(<dir> <repo> <tag>)，已存在则跳过
function(deps_clone dir repo tag)
    if (NOT EXISTS "${DEPS_DIR}/${dir}/.git")
        message(STATUS "cloning ${repo} (${tag})")
        execute_process(
                COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${tag} ${repo} "${DEPS_DIR}/${dir}"
                RESULT_VARIABLE r)
        if (NOT r EQUAL 0)
            message(FATAL_ERROR "clone failed: ${repo}")
        endif ()
    endif ()
endfunction()

# ---- Zygisk API 头文件（仅需 zygisk.hpp，跳过 libcxx 子模块） ----
deps_clone(zygisk-api https://github.com/topjohnwu/zygisk-module-sample.git master)
set(ZYGISK_API_DIR "${DEPS_DIR}/zygisk-api/module/jni")
if (NOT EXISTS "${ZYGISK_API_DIR}/zygisk.hpp")
    message(FATAL_ERROR "zygisk.hpp not found: ${ZYGISK_API_DIR}")
endif ()

# ---- Dobby（无子模块） ----
deps_clone(dobby-src https://github.com/jmpews/Dobby.git master)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS_DIR}/dobby-src" "${CMAKE_BINARY_DIR}/dobby-build")

# ---- LSPlant v6.4（仅拉取 dex_builder 子模块，递归含 parallel-hashmap；
#      避开 .gitmodules 中 git@ 协议的 test/docs 子模块） ----
deps_clone(lsplant-src https://github.com/LSPosed/LSPlant.git v6.4)
execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                -- lsplant/src/main/jni/external/dex_builder
        WORKING_DIRECTORY "${DEPS_DIR}/lsplant-src"
        RESULT_VARIABLE r)
if (NOT r EQUAL 0)
    message(FATAL_ERROR "LSPlant dex_builder submodule fetch failed")
endif ()
set(LSPLANT_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS_DIR}/lsplant-src/lsplant/src/main/jni"
                 "${CMAKE_BINARY_DIR}/lsplant-build")

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
