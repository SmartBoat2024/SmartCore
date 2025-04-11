import subprocess
from pathlib import Path

def get_git_tag():
    try:
        return subprocess.check_output(["git", "describe", "--tags", "--abbrev=0"]).decode().strip()
    except:
        return "vDEV"

def write_version_header(version):
    header_path = Path(__file__).resolve().parent.parent / "src" / "SmartCoreVersion.h"
    with open(header_path, "w") as f:
        f.write(f'#pragma once\n\n#define SMARTCORE_VERSION "{version}"\n')

if __name__ == "__main__":
    version = get_git_tag()
    print(f"[SmartCore] ✅ Detected version: {version}")
    write_version_header(version)