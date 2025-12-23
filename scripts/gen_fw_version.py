#!/usr/bin/env python3
import subprocess
from pathlib import Path

def run(cmd):
    return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode().strip()

def main():
    project_root = Path.cwd()
    out_file = project_root / "include" / "FirmwareVersion.h"
    out_file.parent.mkdir(parents=True, exist_ok=True)

    # ---- Defaults (non-git / initial state) ----
    ota_version = "v@0.0.1-local"
    full_version = "v0.0.1-local"

    if (project_root / ".git").exists():
        try:
            full_version = run(["git", "describe", "--tags", "--dirty", "--always"])
            try:
                tag_version = run(["git", "describe", "--tags", "--abbrev=0"])
                ota_version = f"v@{tag_version.lstrip('v')}"
            except Exception:
                ota_version = "v@0.0.0-dev"
        except Exception:
            # git exists but unusable → keep local fallback
            pass

    header = f"""// AUTO-GENERATED FILE — DO NOT EDIT
#pragma once

#define FW_VER        "{ota_version}"
#define FW_VER_FULL   "{full_version}"
"""

    out_file.write_text(header, encoding="utf-8")

    print(f"[FW_VER] FW_VER={ota_version}")
    print(f"[FW_VER] FW_VER_FULL={full_version}")

main()
