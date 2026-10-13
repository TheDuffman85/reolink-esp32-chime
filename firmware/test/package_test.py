#!/usr/bin/env python3
"""Check release packaging and rejection of missing or stale build artifacts."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools").mkdir()
        shutil.copyfile(TOOLS / "package.py", self.root / "tools/package.py")
        (self.root / "version.json").write_text('{"version": "1.4.2"}\n')
        (self.root / "platformio.ini").write_text("[env:esp32dev]\n")
        build = self.root / ".pio/build/esp32dev"
        build.mkdir(parents=True)
        self.app = build / "firmware.bin"
        self.initial = build / "firmware-initial.bin"
        self.app.write_bytes(b"app\0REOLINK_CHIME_VERSION=1.4.2\0")
        self.initial.write_bytes(b"\xff" * 0x10000 + self.app.read_bytes())

    def package(self):
        return subprocess.run(
            [sys.executable, str(self.root / "tools/package.py")],
            capture_output=True, text=True,
        )

    def test_packages_both_images_with_hashes_and_flash_address(self):
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        dest = self.root / "dist"
        manifest = json.loads((dest / "manifest.json").read_text())
        self.assertEqual(manifest["version"], "1.4.2")
        for source, metadata in ((self.app, manifest), (self.initial, manifest["initial_image"])):
            data = source.read_bytes()
            self.assertEqual((dest / source.name).read_bytes(), data)
            self.assertEqual(metadata["image_bytes"], len(data))
            self.assertEqual(metadata["image_sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(manifest["initial_image"]["file"], self.initial.name)
        self.assertEqual(manifest["initial_image"]["flash_offset"], "0x0")
        self.assertEqual(manifest["source_sha256"]["platformio.ini"],
                         hashlib.sha256((self.root / "platformio.ini").read_bytes()).hexdigest())

    def test_missing_image_fails_before_exporting(self):
        for image in (self.app, self.initial):
            with self.subTest(image=image.name):
                data = image.read_bytes()
                image.unlink()
                result = self.package()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("build_initial", result.stderr)
                self.assertFalse((self.root / "dist").exists())
                image.write_bytes(data)

    def test_stale_version_fails_before_exporting(self):
        (self.root / "version.json").write_text('{"version": "1.4.3"}\n')
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not version 1.4.3", result.stderr)
        self.assertFalse((self.root / "dist").exists())

    def test_stale_merged_app_fails_even_with_same_version(self):
        self.app.write_bytes(self.app.read_bytes() + b"source change")
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does not contain the current app", result.stderr)
        self.assertFalse((self.root / "dist").exists())


if __name__ == "__main__":
    unittest.main()
