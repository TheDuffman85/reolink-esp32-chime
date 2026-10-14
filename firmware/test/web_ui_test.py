"""Verify the exact HTML bytes survive build-time gzip and generated arrays."""
import gzip
from pathlib import Path
import re
import runpy
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_web_ui import generate, pages  # noqa: E402


class WebUiTest(unittest.TestCase):
    def test_real_pages_roundtrip_and_deterministic_incremental_output(self):
        source = ROOT / "include/WebUi.h"
        original = pages(source.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as directory:
            dest = Path(directory)
            sizes = generate(source, dest)
            header = (dest / "WebUiGzip.h").read_text()
            for name, raw in original.items():
                encoded = (dest / (name + ".html.gz")).read_bytes()
                self.assertEqual(gzip.decompress(encoded), raw)
                self.assertEqual(encoded[4:8], b"\0" * 4)  # No build timestamp.
                self.assertEqual(encoded[3], 0)  # No original filename.
                values = re.search(rf"{name}Gzip\[\] PROGMEM = \{{(.*?)\}};", header, re.DOTALL)[1]
                array = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", values))
                self.assertEqual(array, encoded)
                self.assertEqual(sizes[name], (len(raw), len(encoded)))
                self.assertLess(len(encoded), len(raw))
            before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in dest.iterdir()}
            generate(source, dest)
            self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in dest.iterdir()})

    def test_source_changes_regenerate_assets_and_keep_utf8(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "WebUi.h"
            source.write_text('static const char WebUi[] PROGMEM = R"HTML(First — page)HTML";\n'
                              'static const char LoginUi[] PROGMEM = R"HTML(Login)HTML";', encoding="utf-8")
            generate(source, root / "generated")
            before = (root / "generated/WebUiGzip.h").read_bytes()
            source.write_text(source.read_text().replace("First", "Second"), encoding="utf-8")
            generate(source, root / "generated")
            self.assertNotEqual((root / "generated/WebUiGzip.h").read_bytes(), before)
            self.assertEqual(gzip.decompress((root / "generated/WebUi.html.gz").read_bytes()),
                             "Second — page".encode())

    def test_missing_or_duplicate_page_fails_the_build(self):
        with self.assertRaises(ValueError):
            pages("// no HTML")
        literal = 'static const char WebUi[] PROGMEM = R"HTML(page)HTML";'
        with self.assertRaises(ValueError):
            pages(literal + literal)

    def test_platformio_hook_generates_and_registers_build_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)

            class Environment:
                def subst(self, value):
                    return str(ROOT if value == "$PROJECT_DIR" else build)

                def Append(self, **kwargs):
                    self.paths = kwargs["CPPPATH"]

            env = Environment()
            runpy.run_path(str(ROOT / "tools/pio_web_ui.py"),
                           init_globals={"Import": lambda name: None, "env": env})
            self.assertEqual(env.paths, [str(build / "generated")])
            self.assertTrue((build / "generated/WebUiGzip.h").is_file())


if __name__ == "__main__":
    unittest.main()
