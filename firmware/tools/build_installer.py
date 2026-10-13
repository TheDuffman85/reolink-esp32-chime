#!/usr/bin/env python3
"""Build the static USB installer from verified release assets, without compiling."""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil


def build_installer(release_dir, repository, expected_version=None):
    manifest = json.loads((release_dir / "manifest.json").read_text())
    version = manifest["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit("Firmware version must be MAJOR.MINOR.PATCH")
    if expected_version is not None and version != expected_version:
        raise SystemExit("Release tag does not match the firmware manifest version")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise SystemExit("GITHUB_REPOSITORY must be owner/repository")
    metadata = manifest["initial_image"]
    if metadata["file"] != "firmware-initial.bin" or metadata["flash_offset"] != "0x0":
        raise SystemExit("Expected the initial USB image at offset 0x0")
    initial = release_dir / metadata["file"]
    data = initial.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != metadata["image_sha256"] or len(data) != metadata["image_bytes"]:
        raise SystemExit("Initial USB image does not match the release manifest")
    if f"REOLINK_CHIME_VERSION={version}\0".encode() not in data[0x10000:]:
        raise SystemExit("Initial USB image does not contain the manifest version")

    # Keep the web manifest separate from the release hash/metadata manifest.
    # Version + hash URLs prevent cached pages from selecting another build.
    web = release_dir / "web"
    if web.exists():
        shutil.rmtree(web)
    web.mkdir()
    web_source = Path(__file__).resolve().parents[2] / "web"
    suffix = f"{version}-{digest[:12]}"
    web_image = f"firmware-initial-{suffix}.bin"
    web_manifest = f"install-{suffix}.json"
    shutil.copyfile(initial, web / web_image)
    install_manifest = {
        "name": "Reolink ESP32 Chime",
        "version": version,
        "builds": [{
            "chipFamily": "ESP32",
            "parts": [{"path": web_image, "offset": 0}],
        }],
    }
    (web / web_manifest).write_text(json.dumps(install_manifest, indent=2) + "\n")
    page = (web_source / "index.html").read_text()
    for placeholder, value in {
        "__VERSION__": version,
        "__REPOSITORY_URL__": f"https://github.com/{repository}",
        "__INSTALL_MANIFEST__": web_manifest,
        "__INITIAL_IMAGE__": web_image,
    }.items():
        page = page.replace(placeholder, html.escape(value, quote=True))
    (web / "index.html").write_text(page)
    for asset in ("style.css", "installer.js"):
        shutil.copyfile(web_source / asset, web / asset)
    (web / ".nojekyll").touch()
    return web


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release_dir", type=Path)
    parser.add_argument("--expected-version")
    args = parser.parse_args()
    repository = os.environ.get("GITHUB_REPOSITORY", "TheDuffman85/reolink-esp32-chime")
    web = build_installer(args.release_dir, repository, args.expected_version)
    print(f"Packaged browser installer: {web}")
