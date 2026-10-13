"""Build a USB-install image using the same images and settings as USB upload."""
from pathlib import Path
import subprocess

Import("env")


def merge_initial(source, target, env):
    image = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    output = image.with_name("firmware-initial.bin")
    board = env.BoardConfig()
    command = [
        env.subst("$PYTHONEXE"), env.subst("$OBJCOPY"),
        "--chip", board.get("build.mcu"), "merge_bin",
        "--output", str(output), "--target-offset", "0x0",
        "--flash_mode", env.subst("${__get_board_flash_mode(__env__)}"),
        "--flash_freq", env.subst("${__get_board_f_image(__env__)}"),
        "--flash_size", board.get("upload.flash_size", "4MB"),
    ]
    for offset, path in env["FLASH_EXTRA_IMAGES"]:
        command.extend([offset, env.subst(path)])
    command.extend([env.subst("$ESP32_APP_OFFSET"), str(image)])
    subprocess.run(command, check=True)


env.AddCustomTarget(
    name="build_initial",
    dependencies=["$BUILD_DIR/${PROGNAME}.bin"],
    actions=[merge_initial],
    title="Build initial USB image",
    description="Merge bootloader, partitions, OTA boot data and app for flashing at 0x0",
)
