# SmartCore/scripts/version_generator.py

import subprocess

def get_git_tag():
    try:
        return subprocess.check_output(["git", "describe", "--tags", "--abbrev=0"]).decode().strip()
    except:
        return "vDEV"

tag = get_git_tag()
header_path = "lib/SmartCore/src/SmartCoreVersion.h"

with open(header_path, "w") as f:
    f.write(f'#define SMARTCORE_VER "{tag}"\n')

print(f"[SmartCore] ✅ Created SmartCoreVersion.h with version: {tag}")