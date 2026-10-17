#!/usr/bin/env python3
"""Create the drag-to-Applications disk image from the signed release app."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
app = root / "release/NestView.app"
if not app.is_dir():
    raise SystemExit("Run scripts/package-macos.sh first")
subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
with tempfile.TemporaryDirectory(prefix="nestview-installer-", dir=root / "build") as temporary:
    stage = Path(temporary)
    subprocess.run(["ditto", str(app), str(stage / "NestView.app")], check=True)
    (stage / "Applications").symlink_to("/Applications", target_is_directory=True)
    subprocess.run(["hdiutil", "create", "-volname", "NestView", "-srcfolder", str(stage),
                    "-fs", "HFS+", "-format", "UDZO", "-ov",
                    str(root / "release/NestView-macOS-AppleSilicon.dmg")], check=True)
