#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TAL-Patch (Zygisk + WebUI) 打包脚本

前置条件：
  1. ./gradlew :loader:assembleRelease   （产出 classes.jar）
  2. ./gradlew :zygisk:assembleRelease   （产出各 ABI 的 libtalpatch.so）
  3. Android SDK（build-tools 内含 d8）

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
    return m.group(1).strip() if m else "v3.0.0"


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
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if not sdk:
        local = ROOT / "local.properties"
        if local.exists():
            for line in local.read_text(encoding="utf-8").splitlines():
                if line.startswith("sdk.dir="):
                    sdk = line.split("=", 1)[1].strip()
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


def find_so_files() -> dict:
    result = {}
    base = ROOT / "zygisk" / "build" / "intermediates"
    if not base.exists():
        sys.exit("[!] 未找到 zygisk 构建产物，请先执行 ./gradlew :zygisk:assembleRelease")
    for so in base.rglob("libtalpatch.so"):
        parts = [p.lower() for p in so.parts]
        for abi in ("arm64-v8a", "armeabi-v7a", "x86_64", "x86"):
            if abi in parts:
                result[abi] = so
    if not result:
        sys.exit("[!] 未找到 libtalpatch.so")
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
    version = read_version()
    print(f"[*] TAL-Patch {version}")
    package(version)


if __name__ == "__main__":
    main()
