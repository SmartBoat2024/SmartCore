Import("env")

env.AddCustomTarget(
    name="erasefs",
    dependencies=None,
    actions=[
        lambda target, source, env: env.Execute(
            f"{env['PYTHONEXE']} {env['PROJECT_PACKAGES_DIR']}/tool-esptoolpy/esptool.py "
            f"--chip esp32s3 --port {env['UPLOAD_PORT']} erase_region 0x670000 0x180000"
        )
    ],
    title="Erase FS",
    description="Erases the LittleFS partition"
)
