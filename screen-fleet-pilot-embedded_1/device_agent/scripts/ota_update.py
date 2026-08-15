#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""OTA install: extract firmware tgz and replace executables."""

import os
import shutil
import stat
import sys
import tarfile
import tempfile

PROJECT_DIR = "/home/root/Projects/screen-fleet-pilot-embedded_1"
BINARIES = ("device-agent", "screen-fleet-pilot-embedded")


def chmod_exec(path):
    mode = os.stat(path).st_mode
    os.chmod(path, mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def main():
    if len(sys.argv) < 2:
        print("usage: ota_update.py <firmware.tgz>")
        return 1

    tgz = sys.argv[1]
    if not os.path.isfile(tgz):
        print("ERROR: firmware not found: %s" % tgz)
        return 1

    for name in BINARIES:
        old = os.path.join(PROJECT_DIR, name)
        if os.path.exists(old):
            os.remove(old)
            print("removed %s" % old)

    tmp = tempfile.mkdtemp(prefix="ota-")
    try:
        tar = tarfile.open(tgz, "r:gz")
        try:
            tar.extractall(tmp)
        finally:
            tar.close()

        for name in BINARIES:
            src = os.path.join(tmp, name)
            if not os.path.isfile(src):
                print("ERROR: missing in archive: %s" % name)
                return 1

            dst = os.path.join(PROJECT_DIR, name)
            shutil.copy2(src, dst)
            chmod_exec(dst)
            print("installed %s" % dst)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("OTA install ok")
    os.system("reboot")
    return 0


if __name__ == "__main__":
    sys.exit(main())
