"""PlatformIO pre-build hook for gzip pages; no compression runs on the ESP32."""
Import("env")  # noqa: F821
from pathlib import Path
import sys

project = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
sys.path.insert(0, str(project / "tools"))
from build_web_ui import generate  # noqa: E402

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
sizes = generate(project / "include/WebUi.h", generated)
env.Append(CPPPATH=[str(generated)])  # noqa: F821
print("Web pages gzip: " + ", ".join(f"{name} {raw} -> {encoded} bytes" for name, (raw, encoded) in sizes.items()))
