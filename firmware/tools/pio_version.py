# PlatformIO pre-build script: defines FIRMWARE_VERSION from version.json,
# without changing the manually configured version.
Import("env")  # noqa: F821 (provided by SCons)
import sys
from pathlib import Path

sys.path.insert(0, str(Path(env.subst("$PROJECT_DIR")) / "tools"))  # noqa: F821
import version  # noqa: E402

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version.build_version()))])  # noqa: F821
