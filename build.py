#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TAL-Patch (Zygisk + WebUI) 打包脚本

前置条件：
  1. ./gradlew :loader:assembleRelease   （产出 classes.jar）
  2. Android SDK（build-tools 内含 d8）与 NDK r26+
  3. CMake 3.28 ~ 3.31（DexBuilder 需要 >= 3.28，Dobby 与 CMake 4.x 不兼容）
     —— 本脚本会直接调用 CMake 构建 native 层（可用 --skip-native 跳过）

产出：dist/tal_patch-<version>.zip（KernelSU / Magisk 可刷入）

Author: Kevin233 (https://github.com/Kevin233B) & yangyang8002 (https://github.com/yangyang8002)
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MODULE_DIR = ROOT / "module"
DIST_DIR = ROOT / "dist"


def read_version() -> str:
    prop = (MODULE_DIR / "module.prop").read_text(encoding="utf-8")
    m = re.search(r"^version=(.+)$", prop, re.MULTILINE)
    return m.group(1).strip() if m else "v26.9.1"


def find_classes_jar() -> Path:
    candidates = sorted(
        (ROOT / "loader" / "build" / "intermediates").rglob("*.jar"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    for jar in candidates:
        if "classes" in jar.name or "bundle" in jar.name:
            return jar
    if candidates:
        return candidates[0]
    sys.exit("[!] 未找到 loader classes.jar，请先执行 ./gradlew :loader:assembleRelease")


def find_d8() -> Path:
    sdk = find_sdk()
    if not sdk:
        sys.exit("[!] 未设置 ANDROID_HOME / sdk.dir")
    build_tools = sorted(
        (Path(sdk) / "build-tools").iterdir(), reverse=True
    )
    for bt in build_tools:
        for name in ("d8", "d8.bat", "d8.exe"):
            d8 = bt / name
            if d8.exists():
                return d8
    sys.exit("[!] build-tools 中未找到 d8")


def build_dex(out_dir: Path) -> Path:
    classes_jar = find_classes_jar()
    d8 = find_d8()
    print(f"[*] d8: {classes_jar}")
    subprocess.run(
        [str(d8), "--release", "--min-api", "26",
         "--output", str(out_dir), str(classes_jar)],
        check=True,
    )
    dex = out_dir / "classes.dex"
    if not dex.exists():
        sys.exit("[!] d8 未产出 classes.dex")
    target = out_dir / "loader.dex"
    dex.rename(target)
    return target


def find_sdk() -> Path | None:
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if not sdk:
        local = ROOT / "local.properties"
        if local.exists():
            for line in local.read_text(encoding="utf-8").splitlines():
                if line.startswith("sdk.dir="):
                    sdk = line.split("=", 1)[1].strip()
    return Path(sdk) if sdk else None


def find_ndk() -> Path:
    for env in ("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT", "ANDROID_NDK"):
        if os.environ.get(env):
            return Path(os.environ[env])
    sdk = find_sdk()
    if sdk:
        ndk_dir = sdk / "ndk"
        if ndk_dir.exists():
            versions = sorted(ndk_dir.iterdir(), reverse=True)
            if versions:
                return versions[0]
        bundle = sdk / "ndk-bundle"
        if bundle.exists():
            return bundle
    sys.exit("[!] 未找到 NDK，请设置 ANDROID_NDK_HOME 或安装 ndk 到 SDK/ndk")


def check_cmake() -> None:
    try:
        out = subprocess.run(["cmake", "--version"], capture_output=True,
                             text=True, check=True).stdout
        ver = tuple(int(x) for x in re.search(r"version (\d+)\.(\d+)", out).groups())
        if not ((3, 28) <= ver < (4, 0)):
            sys.exit(f"[!] CMake 版本 {ver[0]}.{ver[1]} 不在 3.28~3.31 范围内")
    except FileNotFoundError:
        sys.exit("[!] 未找到 cmake，可 pip install 'cmake==3.31.*'")


def build_native(abis: list) -> None:
    check_cmake()
    ndk = find_ndk()
    toolchain = ndk / "build" / "cmake" / "android.toolchain.cmake"
    if not toolchain.exists():
        sys.exit(f"[!] NDK toolchain 不存在: {toolchain}")
    generator = ["-G", "Ninja"] if shutil.which("ninja") else []
    for abi in abis:
        build_dir = ROOT / "zygisk" / "build-native" / abi
        print(f"[*] native 构建: {abi} (NDK {ndk.name})")
        subprocess.run(
            ["cmake", "-S", str(ROOT / "zygisk"), "-B", str(build_dir),
             *generator,
             f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
             "-DANDROID_ABI=" + abi,
             "-DANDROID_PLATFORM=android-26",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DANDROID_STL=c++_static"],
            check=True,
        )
        subprocess.run(["cmake", "--build", str(build_dir), "--target", "talpatch"],
                       check=True)


def find_so_files() -> dict:
    result = {}
    for base in (ROOT / "zygisk" / "build-native",
                 ROOT / "zygisk" / "build" / "intermediates"):
        if not base.exists():
            continue
        for so in base.rglob("libtalpatch.so"):
            parts = [p.lower() for p in so.parts]
            for abi in ("arm64-v8a", "armeabi-v7a", "x86_64", "x86"):
                if abi in parts:
                    result[abi] = so
    if not result:
        sys.exit("[!] 未找到 libtalpatch.so，请先构建 native 层")
    return result


def package(version: str) -> Path:
    DIST_DIR.mkdir(exist_ok=True)
    so_files = find_so_files()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        dex = build_dex(tmp)

        staging = tmp / "staging"
        shutil.copytree(MODULE_DIR, staging)
        shutil.copy2(dex, staging / "loader.dex")
        zy_dir = staging / "zygisk"
        zy_dir.mkdir(exist_ok=True)
        for abi, so in so_files.items():
            shutil.copy2(so, zy_dir / f"{abi}.so")
            print(f"[*] zygisk/{abi}.so")

        out_zip = DIST_DIR / f"tal_patch-{version}.zip"
        if out_zip.exists():
            out_zip.unlink()
        with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zf:
            for file in sorted(staging.rglob("*")):
                if file.is_file():
                    zf.write(file, file.relative_to(staging).as_posix())
        print(f"[+] 打包完成: {out_zip} ({out_zip.stat().st_size / 1024:.1f} KiB)")
        return out_zip


def main() -> None:
    skip_native = "--skip-native" in sys.argv
    version = read_version()
    print(f"[*] TAL-Patch {version}")
    if not skip_native:
        build_native(["arm64-v8a"])
    package(version)


if __name__ == "__main__":
    main()
