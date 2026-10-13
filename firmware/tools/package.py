#!/usr/bin/env python3
"""Package the last cross-build as OTA and initial USB images plus hashes."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
image = root / ".pio/build/esp32dev/firmware.bin"
initial = image.with_name("firmware-initial.bin")
if not image.is_file() or not initial.is_file():
    raise SystemExit("Run pio run -e esp32dev -t build_initial in firmware/ first")
version = json.loads((root / "version.json").read_text())["version"]
# The image must carry the same version (stale build or edited version.json otherwise).
if f"REOLINK_CHIME_VERSION={version}\0".encode() not in image.read_bytes():
    raise SystemExit(f"firmware.bin is not version {version}; run pio run -t build_initial first")
if initial.read_bytes()[0x10000:] != image.read_bytes():
    raise SystemExit("firmware-initial.bin does not contain the current app; run pio run -t build_initial first")
dest = root / "dist"
dest.mkdir(exist_ok=True)
shutil.copyfile(image, dest / "firmware.bin")
shutil.copyfile(initial, dest / initial.name)
inputs = sorted([root / "platformio.ini", *root.glob("include/*.h"), *root.glob("src/*.cpp")])
manifest = {
    "version": version,
    "board": "esp32dev (ESP32-WROOM-32, 4 MB flash)",
    "platform": "espressif32@6.9.0",
    "arduino_core": "2.0.17",
    "arduino_json": "6.21.5",
    "settings_schema": 2,
    "image_type": "OTA app only; initial USB flash requires bootloader and partition table",
    "pairing": "pairing/join validated on owner hardware; signed-positive chime IDs",
    "rf": "868 MHz CC1101 RX/TX validated on owner hardware; PATABLE 0xC0 (about +11 dBm). 915/920 MHz presets (902-928 MHz, PATABLE 0x8E, about 0 dBm) untested",
    "image_bytes": image.stat().st_size,
    "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
    "initial_image": {
        "file": initial.name,
        "image_type": "Initial USB install: bootloader, partition table, OTA boot data and app",
        "flash_offset": "0x0",
        "image_bytes": initial.stat().st_size,
        "image_sha256": hashlib.sha256(initial.read_bytes()).hexdigest(),
    },
    "source_sha256": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
}
(dest / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Packaged version {version}: {manifest['image_bytes']} app bytes and {initial.stat().st_size} initial USB bytes")
