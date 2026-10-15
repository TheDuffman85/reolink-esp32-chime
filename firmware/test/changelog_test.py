#!/usr/bin/env python3
"""Regression checks for release notes taken from CHANGELOG.md."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


FIRMWARE = Path(__file__).resolve().parents[1]
CHANGELOG = """# Changelog

Introduction mentioning 1.4.2.

## [1.4.3] - 2026-01-02

### Fixed

- Newer fix.

## 1.4.2 - 2026-01-01

### Added

- Feature A.

## 1.4.20

- Different version.

## 1.4.1
"""


class ChangelogTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "firmware/tools").mkdir(parents=True)
        for name in ("changelog.py", "version.py"):
            shutil.copyfile(FIRMWARE / "tools" / name, self.root / "firmware/tools" / name)
        (self.root / "firmware/version.json").write_text('{"version": "1.4.2"}\n')
        self.changelog = self.root / "CHANGELOG.md"
        self.changelog.write_text(CHANGELOG)

    def cli(self, *args):
        return subprocess.run(
            [sys.executable, str(self.root / "firmware/tools/changelog.py"), *args],
            capture_output=True, text=True,
        )

    def test_defaults_to_firmware_version(self):
        result = self.cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "### Added\n\n- Feature A.\n")

    def test_explicit_and_bracketed_versions(self):
        result = self.cli("1.4.3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "### Fixed\n\n- Newer fix.\n")
        result = self.cli("1.4.20")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "- Different version.\n")

    def test_missing_version_fails(self):
        for version in ("1.4.4", "1.4", "1.4.2.0"):
            with self.subTest(version=version):
                result = self.cli(version)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"no section for version {version}", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_empty_section_fails(self):
        result = self.cli("1.4.1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("section for version 1.4.1 is empty", result.stderr)

    def test_missing_changelog_fails(self):
        self.changelog.unlink()
        result = self.cli()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("CHANGELOG.md not found", result.stderr)


if __name__ == "__main__":
    unittest.main()
