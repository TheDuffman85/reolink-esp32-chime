#!/usr/bin/env python3
"""Check release packaging and rejection of missing or stale build artifacts."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"
WEB = TOOLS.parent.parent / "web"


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "firmware"
        self.root.mkdir()
        (self.root / "tools").mkdir()
        shutil.copyfile(TOOLS / "package.py", self.root / "tools/package.py")
        shutil.copyfile(TOOLS / "build_installer.py", self.root / "tools/build_installer.py")
        shutil.copytree(WEB, self.root.parent / "web")
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
            env={**os.environ, "GITHUB_REPOSITORY": "example/chime"},
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

    def test_installer_uses_matching_initial_image_at_zero_on_project_pages(self):
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        web = self.root / "dist/web"
        page = (web / "index.html").read_text()
        self.assertNotIn("__VERSION__", page)
        self.assertNotIn("__REPOSITORY_URL__", page)
        self.assertIn("https://github.com/example/chime/releases/tag/1.4.2", page)
        manifests = list(web.glob("install-*.json"))
        self.assertEqual(len(manifests), 1)
        self.assertIn(f'manifest="{manifests[0].name}"', page)
        install = json.loads(manifests[0].read_text())
        self.assertEqual(install["version"], "1.4.2")
        self.assertEqual(len(install["builds"]), 1)
        self.assertEqual(install["builds"][0]["chipFamily"], "ESP32")
        parts = install["builds"][0]["parts"]
        self.assertEqual(len(parts), 1)
        self.assertEqual(parts[0]["offset"], 0)
        self.assertEqual((web / parts[0]["path"]).read_bytes(), self.initial.read_bytes())
        self.assertIn(hashlib.sha256(self.initial.read_bytes()).hexdigest()[:12], parts[0]["path"])
        for asset in ("style.css", "installer.js", ".nojekyll"):
            self.assertTrue((web / asset).is_file())

    def test_repackaging_replaces_old_installer_image_and_manifest(self):
        self.assertEqual(self.package().returncode, 0)
        web = self.root / "dist/web"
        old_manifest = next(web.glob("install-*.json"))
        old_image = next(web.glob("*.bin"))
        self.app.write_bytes(self.app.read_bytes() + b"new build")
        self.initial.write_bytes(b"\xff" * 0x10000 + self.app.read_bytes())
        self.assertEqual(self.package().returncode, 0)
        self.assertFalse(old_manifest.exists())
        self.assertFalse(old_image.exists())
        self.assertEqual(len(list(web.glob("*.bin"))), 1)

    def test_builds_from_release_assets_without_a_build_cache(self):
        self.assertEqual(self.package().returncode, 0)
        shutil.rmtree(self.root / ".pio")
        shutil.rmtree(self.root / "dist/web")
        result = subprocess.run(
            [sys.executable, str(self.root / "tools/build_installer.py"),
             str(self.root / "dist"), "--expected-version", "1.4.2"],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.root / "dist/web/index.html").is_file())

    def test_release_installer_rejects_wrong_tag_and_corrupt_image(self):
        self.assertEqual(self.package().returncode, 0)
        shutil.rmtree(self.root / "dist/web")
        command = [sys.executable, str(self.root / "tools/build_installer.py"),
                   str(self.root / "dist")]
        result = subprocess.run(command + ["--expected-version", "1.4.3"],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("tag does not match", result.stderr)
        self.assertFalse((self.root / "dist/web").exists())
        (self.root / "dist/firmware-initial.bin").write_bytes(b"corrupt image")
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does not match the release manifest", result.stderr)
        self.assertFalse((self.root / "dist/web").exists())

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
