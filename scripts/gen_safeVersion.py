#!/usr/bin/env python3
from pathlib import Path


# set version to v@0.0.2 so bootsafe version set to v@0.0.1 will download safe firmware

def main():
    project_root = Path.cwd()
    out_file = project_root / "include" / "FirmwareVersion.h"
    out_file.parent.mkdir(parents=True, exist_ok=True)

    header = """// AUTO-GENERATED SAFE FIRMWARE VERSION — DO NOT EDIT
#pragma once

#define FW_VER        "v@0.0.2"
#define FW_VER_FULL   "SAFE-RECOVERY"
"""

    out_file.write_text(header, encoding="utf-8")

    print("[FW_VER] SAFE firmware version forced to v@0.0.2")

main()
