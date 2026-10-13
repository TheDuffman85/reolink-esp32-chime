# PlatformIO pre-build script: defines FIRMWARE_VERSION from version.json,
# bumping the patch number when the firmware sources changed.
Import("env")  # noqa: F821 (provided by SCons)
import sys
from pathlib import Path

sys.path.insert(0, str(Path(env.subst("$PROJECT_DIR")) / "tools"))  # noqa: F821
import version  # noqa: E402

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version.build_version()))])  # noqa: F821
