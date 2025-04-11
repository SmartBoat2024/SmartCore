import subprocess

def get_git_tag():
    try:
        return subprocess.check_output(["git", "describe", "--tags", "--abbrev=0"]).decode().strip()
    except:
        return "vDEV"

Import("env")
tag = get_git_tag()

# 🔧 This ensures the macro is seen as a proper string: "v0.0.1"
env.Append(
    CPPDEFINES=[
        ("FW_VER", '\\"{}\\"'.format(tag))  # Escaped for CLI preprocessor
    ]
)