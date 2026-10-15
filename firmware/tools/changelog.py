#!/usr/bin/env python3
"""Print the CHANGELOG.md section of a firmware version.

    python3 tools/changelog.py           # section of the version.json version
    python3 tools/changelog.py 1.4.2     # section of 1.4.2

Fails when the section is missing or empty, so a release cannot be published
without documented changes.
"""
import re
import sys
from pathlib import Path

from version import build_version

CHANGELOG = Path(__file__).resolve().parents[2] / "CHANGELOG.md"


def section(version, text):
    """Return the stripped body under '## <version>' up to the next '## ' heading."""
    heading = re.compile(rf"## \[?{re.escape(version)}\]?(?:[ \t].*)?")
    lines = text.splitlines()
    for start, line in enumerate(lines):
        if heading.fullmatch(line.rstrip()):
            body = []
            for line in lines[start + 1:]:
                if line.startswith("## "):
                    break
                body.append(line)
            return "\n".join(body).strip()
    return None


def main(args):
    if len(args) > 1:
        raise SystemExit(__doc__)
    version = args[0] if args else build_version()
    if not CHANGELOG.is_file():
        raise SystemExit(f"{CHANGELOG} not found")
    notes = section(version, CHANGELOG.read_text())
    if notes is None:
        raise SystemExit(f"{CHANGELOG.name} has no section for version {version}")
    if not notes:
        raise SystemExit(f"{CHANGELOG.name} section for version {version} is empty")
    print(notes)


if __name__ == "__main__":
    main(sys.argv[1:])
