#!/usr/bin/env python3
"""Regression checks for manual versioning, using an isolated project copy."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1] / "tools"


class VersionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools").mkdir()
        for name in ("version.py", "pio_version.py"):
            shutil.copyfile(TOOLS / name, self.root / "tools" / name)
        self.state = self.root / "version.json"
        self.state.write_text('{"version": "1.4.2"}\n')

    def cli(self, *args):
        return subprocess.run(
            [sys.executable, str(self.root / "tools/version.py"), *args],
            capture_output=True, text=True,
        )

    def test_build_hook_keeps_version_after_source_changes(self):
        before = self.state.read_bytes()
        # Execute the real pre-build hook with the small SCons API it uses.
        hook = """
import runpy
import sys
class Environment:
    def subst(self, value):
        return sys.argv[1]
    def StringifyMacro(self, value):
        return '"' + value + '"'
    def Append(self, **kwargs):
        assert kwargs['CPPDEFINES'] == [('FIRMWARE_VERSION', '"1.4.2"')]
runpy.run_path(sys.argv[1] + '/tools/pio_version.py',
               init_globals={'Import': lambda name: None, 'env': Environment()})
"""
        for content in ("original sources", "changed sources"):
            (self.root / "src").mkdir(exist_ok=True)
            (self.root / "src/main.cpp").write_text(content)
            result = subprocess.run(
                [sys.executable, "-c", hook, str(self.root)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(self.state.read_bytes(), before)
            self.assertEqual(self.cli().stdout.strip(), "1.4.2")

    def test_explicit_bumps(self):
        for part, expected in (("patch", "1.4.3"), ("minor", "1.5.0"), ("major", "2.0.0")):
            with self.subTest(part=part):
                self.state.write_text('{"version": "1.4.2"}\n')
                result = self.cli(part)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), expected)
                self.assertEqual(json.loads(self.state.read_text()), {"version": expected})

    def test_invalid_versions_are_rejected_without_writes(self):
        for value in ("v1.4.2", "1.4", "1.4.2-rc1", "01.4.2", "1.4.2\n", 142, None):
            with self.subTest(version=value):
                self.state.write_text(json.dumps({"version": value}))
                before = self.state.read_bytes()
                result = self.cli()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("version must look like 1.2.3", result.stderr)
                self.assertEqual(self.state.read_bytes(), before)

    def test_unknown_command_does_not_change_version(self):
        before = self.state.read_bytes()
        self.assertNotEqual(self.cli("automatic").returncode, 0)
        self.assertEqual(self.state.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
