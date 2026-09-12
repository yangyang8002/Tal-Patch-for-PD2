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

# 浅克隆辅助：deps_clone(<dir> <repo> <ref>)，支持分支/tag/commit sha，已存在则跳过
function(deps_clone dir repo ref)
    if (NOT EXISTS "${DEPS_DIR}/${dir}/.git")
        message(STATUS "cloning ${repo} (${ref})")
        execute_process(COMMAND ${GIT_EXECUTABLE} init "${DEPS_DIR}/${dir}"
                RESULT_VARIABLE r)
        if (r EQUAL 0)
            execute_process(COMMAND ${GIT_EXECUTABLE} fetch --depth 1 ${repo} ${ref}
                    WORKING_DIRECTORY "${DEPS_DIR}/${dir}" RESULT_VARIABLE r)
        endif ()
        if (r EQUAL 0)
            execute_process(COMMAND ${GIT_EXECUTABLE} checkout --detach FETCH_HEAD
                    WORKING_DIRECTORY "${DEPS_DIR}/${dir}"
                    RESULT_VARIABLE r OUTPUT_QUIET ERROR_QUIET)
        endif ()
        if (NOT r EQUAL 0)
            file(REMOVE_RECURSE "${DEPS_DIR}/${dir}")
            message(FATAL_ERROR "clone failed: ${repo} @ ${ref}")
        endif ()
    endif ()
endfunction()

# ---- Zygisk API 头文件（仅需 zygisk.hpp，跳过 libcxx 子模块） ----
deps_clone(zygisk-api https://github.com/topjohnwu/zygisk-module-sample.git master)
set(ZYGISK_API_DIR "${DEPS_DIR}/zygisk-api/module/jni")
if (NOT EXISTS "${ZYGISK_API_DIR}/zygisk.hpp")
    message(FATAL_ERROR "zygisk.hpp not found: ${ZYGISK_API_DIR}")
endif ()

# ---- Dobby（使用 LSPosed 维护的修复分支，master 原版在新 NDK 下无法编译） ----
deps_clone(dobby-src https://github.com/LSPosed/Dobby.git 6813ca76ddeafcaece525bf8c6cde7ff4c21d3ce)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)

# Dobby 的 arm64 汇编在 Apple 分支误用了 Mach-O 的 @PAGE/@PAGEOFF 写法，
# ELF(Android) 下 LLVM IAS 拒绝汇编（invalid symbol kind for ADRP relocation）。
# 这里在配置期将其改写为 ELF 标准写法（adrp + :lo12:），可重复执行。
set(DOBBY_CLOSURE_ASM
        "${DEPS_DIR}/dobby-src/source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm")
if (EXISTS "${DOBBY_CLOSURE_ASM}")
    file(READ "${DOBBY_CLOSURE_ASM}" asm_src)
    string(REPLACE "adrp TMP_REG_0, cdecl(common_closure_bridge_handler)@PAGE"
            "adrp TMP_REG_0, cdecl(common_closure_bridge_handler)" asm_src "${asm_src}")
    string(REPLACE "add TMP_REG_0, TMP_REG_0, cdecl(common_closure_bridge_handler)@PAGEOFF"
            "add TMP_REG_0, TMP_REG_0, :lo12:cdecl(common_closure_bridge_handler)" asm_src "${asm_src}")
    file(WRITE "${DOBBY_CLOSURE_ASM}" "${asm_src}")
endif ()
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

target_include_directories(talpatch PRIVATE
        "${ZYGISK_API_DIR}"
        "${DEPS_DIR}/dobby-src/include")
set_target_properties(talpatch PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_options(talpatch PRIVATE -fvisibility=hidden -fno-rtti)
target_link_options(talpatch PRIVATE -Wl,--exclude-libs,ALL)

target_link_libraries(talpatch PRIVATE lsplant_static dobby log)
