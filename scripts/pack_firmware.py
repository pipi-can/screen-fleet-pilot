#!/usr/bin/env python3
"""打包 OTA 固件：两个可执行文件 + README.json → .tgz"""

import argparse
import json
import os
import shutil
import stat
import tarfile
import tempfile
from datetime import datetime

AGENT_BIN = "/home/songcan/Desktop/screen-fleet-pilot/screen-fleet-pilot-embedded/device-agent/build/device-agent"
EMBEDDED_BIN = "/home/songcan/Desktop/screen-fleet-pilot/screen-fleet-pilot-embedded/screen-fleet-pilot-embedded"
OUT_DIR = "/home/songcan/Desktop/screen-fleet-pilot/dist"

EXECUTABLES = [
    ("device-agent", AGENT_BIN),
    ("screen-fleet-pilot-embedded", EMBEDDED_BIN),
]


def build_readme(version: str, changelog: str) -> dict:
    files = []
    for name, path in EXECUTABLES:
        files.append({"name": name, "size": os.path.getsize(path)})

    return {
        "version": version,
        "executables": [name for name, _ in EXECUTABLES],
        "pack_time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "files": files,
        "changelog": changelog,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="打包 screen-fleet OTA 固件")
    parser.add_argument("--version", required=True, help="固件版本号，如 1.0.0")
    parser.add_argument("--changelog", required=True, help="更新说明，可用 \\n 表示换行")
    args = parser.parse_args()

    version = args.version.strip()
    changelog = args.changelog.replace("\\n", "\n")

    for name, path in EXECUTABLES:
        if not os.path.isfile(path):
            print(f"ERROR: missing executable: {name} -> {path}")
            return 1

    readme = build_readme(version, changelog)
    pkg_name = f"screen-fleet-pilot-firmware-v{version}"
    staging = os.path.join(tempfile.gettempdir(), pkg_name)

    if os.path.isdir(staging):
        shutil.rmtree(staging)
    os.makedirs(staging)

    try:
        for name, src in EXECUTABLES:
            dst = os.path.join(staging, name)
            shutil.copy2(src, dst)
            mode = os.stat(dst).st_mode
            os.chmod(dst, mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

        with open(os.path.join(staging, "README.json"), "w", encoding="utf-8") as f:
            json.dump(readme, f, ensure_ascii=False, indent=2)

        os.makedirs(OUT_DIR, exist_ok=True)
        out_tgz = os.path.join(OUT_DIR, f"{pkg_name}.tgz")

        with tarfile.open(out_tgz, "w:gz") as tar:
            for item in sorted(os.listdir(staging)):
                tar.add(os.path.join(staging, item), arcname=item)

        print(f"==> Version: {version}")
        print(f"==> Pack time: {readme['pack_time']}")
        print(f"==> Done: {out_tgz}")
        for info in readme["files"]:
            print(f"    {info['name']}: {info['size']} bytes")
        return 0
    finally:
        if os.path.isdir(staging):
            shutil.rmtree(staging)


if __name__ == "__main__":
    raise SystemExit(main())

