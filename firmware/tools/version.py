#!/usr/bin/env python3
"""Manually managed firmware version (version.json).

Builds only read the configured version. Edit version.json directly or bump
one part explicitly:

    python3 tools/version.py           # show the version
    python3 tools/version.py patch     # 1.4.2 -> 1.4.3
    python3 tools/version.py minor     # 1.4.2 -> 1.5.0
    python3 tools/version.py major     # 1.4.2 -> 2.0.0
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / "version.json"


def load():
    state = json.loads(STATE.read_text())
    version = state.get("version")
    if not isinstance(version, str) or not re.fullmatch(
        r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version
    ):
        raise SystemExit(f"{STATE}: version must look like 1.2.3")
    return state


def save(version):
    STATE.write_text(json.dumps({"version": version}, indent=2) + "\n")


def build_version():
    """Read the configured version without changing any files."""
    return load()["version"]


def bump(part):
    major, minor, patch = map(int, load()["version"].split("."))
    if part == "major":
        version = f"{major + 1}.0.0"
    elif part == "minor":
        version = f"{major}.{minor + 1}.0"
    elif part == "patch":
        version = f"{major}.{minor}.{patch + 1}"
    else:
        raise ValueError(f"Unknown version part: {part}")
    save(version)
    return version


if __name__ == "__main__":
    if len(sys.argv) == 1:
        print(load()["version"])
    elif sys.argv[1:] in (["patch"], ["minor"], ["major"]):
        print(bump(sys.argv[1]))
    else:
        raise SystemExit(__doc__)
