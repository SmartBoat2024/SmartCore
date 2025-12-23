#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def run(cmd):
    return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode().strip()

try:
    tag_version = run(["git", "describe", "--tags", "--abbrev=0"])
    full_version = run(["git", "describe", "--tags", "--dirty", "--always"])
except Exception as e:
    print(f"[SmartCore] ❌ Failed to determine version: {e}")
    sys.exit(1)

out_file = Path(__file__).resolve().parent.parent / "src" / "SmartCoreVersion.h"

header = f"""// AUTO-GENERATED FILE — DO NOT EDIT
#pragma once

#define SMARTCORE_VERSION        "{tag_version}"
#define SMARTCORE_VERSION_FULL   "{full_version}"
"""

out_file.parent.mkdir(parents=True, exist_ok=True)
out_file.write_text(header, encoding="utf-8")

print(f"[SmartCore] ✅ SMARTCORE_VERSION={tag_version}")
print(f"[SmartCore] 📦 SMARTCORE_VERSION_FULL={full_version}")
