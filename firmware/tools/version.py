#!/usr/bin/env python3
"""Firmware version bookkeeping (version.json).

The patch number goes up automatically when a build sees changed firmware
sources (platformio.ini, include/*.h, src/*.cpp); rebuilding unchanged code
keeps the version. Bump the minor or major number by hand:

    python3 tools/version.py           # show the version
    python3 tools/version.py minor     # 1.4.2 -> 1.5.0
    python3 tools/version.py major     # 1.4.2 -> 2.0.0
"""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / "version.json"


def source_hash():
    digest = hashlib.sha256()
    for path in sorted([ROOT / "platformio.ini", *ROOT.glob("include/*.h"), *ROOT.glob("src/*.cpp")]):
        digest.update(str(path.relative_to(ROOT)).encode() + b"\0" + path.read_bytes() + b"\0")
    return digest.hexdigest()


def load():
    state = json.loads(STATE.read_text())
    if not re.fullmatch(r"\d+\.\d+\.\d+", state.get("version", "")):
        raise SystemExit(f"{STATE}: version must look like 1.2.3")
    return state


def save(version, sources):
    STATE.write_text(json.dumps({"version": version, "source_sha256": sources}, indent=2) + "\n")


def build_version():
    """Version for the build about to run; bumps the patch if sources changed."""
    state, sources = load(), source_hash()
    if state.get("source_sha256") != sources:
        major, minor, patch = map(int, state["version"].split("."))
        state["version"] = f"{major}.{minor}.{patch + 1}"
        save(state["version"], sources)
        print(f"Firmware sources changed: version {state['version']}")
    return state["version"]


def bump(part):
    major, minor, patch = map(int, load()["version"].split("."))
    version = f"{major + 1}.0.0" if part == "major" else f"{major}.{minor + 1}.0"
    save(version, source_hash())  # The next build of these sources keeps it.
    return version


if __name__ == "__main__":
    if len(sys.argv) == 1:
        print(load()["version"])
    elif sys.argv[1:] in (["minor"], ["major"]):
        print(bump(sys.argv[1]))
    else:
        raise SystemExit(__doc__)
