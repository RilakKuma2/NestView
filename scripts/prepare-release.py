#!/usr/bin/env python3
"""Prepare signed GitHub release assets; never publishes or exports keys."""
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import platform
import sys

root = Path(__file__).resolve().parent.parent
repo = os.environ.get("NESTVIEW_UPDATE_REPOSITORY", "RilakKuma2/NestView")
if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo) or repo.lower().startswith("jurplel/"):
    raise SystemExit("Set NESTVIEW_UPDATE_REPOSITORY to your own GitHub owner/repository.")

subprocess.run([str(root / "scripts/build-macos.sh")], cwd=root, check=True)
tools = root / "build/_deps/photo_sparkle-src/bin"
with (root / "build/NestView.app/Contents/Info.plist").open("rb") as stream:
    info = plistlib.load(stream)
version = info["CFBundleShortVersionString"]
if not re.fullmatch(r"\d+(?:\.\d+){1,2}", version):
    raise SystemExit("Use a numeric project version.")
public_key = subprocess.check_output(
    [str(tools / "generate_keys"), "--account", "org.nestview.viewer", "-p"], text=True).strip()
if public_key != info["SUPublicEDKey"]:
    raise SystemExit("The Keychain key does not match this app. Restore the original key.")
expected_feed = f"https://github.com/{repo}/releases/latest/download/appcast.xml"
if info.get("SUFeedURL") != expected_feed:
    raise SystemExit("The built app has a different update repository.")
output = root / "release/updates" / f"v{version}"
if output.exists():
    raise SystemExit(f"Move existing release assets aside first: {output}")
subprocess.run([str(root / "scripts/package-macos.sh")], cwd=root, check=True)
output.mkdir(parents=True)
archive = output / f"NestView-{version}-{platform.machine()}.zip"
shutil.copy2(root / "release/NestView-macOS-AppleSilicon.zip", archive)
subprocess.run([str(tools / "generate_appcast"), "--account", "org.nestview.viewer",
    "--download-url-prefix", f"https://github.com/{repo}/releases/download/v{version}/",
    "--link", f"https://github.com/{repo}/releases/tag/v{version}",
    "--maximum-deltas", "0", str(output)], check=True)
subprocess.run([str(tools / "sign_update"), "--account", "org.nestview.viewer",
    "--verify", str(output / "appcast.xml")], check=True)
subprocess.run([sys.executable, str(root / "scripts/package-dmg.py")], cwd=root, check=True)
dmg = root / "release/NestView-macOS-AppleSilicon.dmg"
subprocess.run(["hdiutil", "verify", str(dmg)], check=True)
shutil.copy2(dmg, output / dmg.name)
print(f"Upload the DMG, ZIP, and appcast.xml from {output} to the matching GitHub Release.")
