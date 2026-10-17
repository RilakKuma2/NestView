#!/usr/bin/env python3
"""Deploy a native UCRT64 build, Qt plugins and their transitive runtime DLLs."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parent.parent
executable = root / "build-windows/NestView.exe"
qt_deploy = shutil.which("windeployqt6") or shutil.which("windeployqt")
objdump = shutil.which("objdump")
if not qt_deploy or not objdump or not executable.is_file():
    raise SystemExit("Use MSYS2 UCRT64 with Qt, binutils, and a built NestView.exe.")
qt_bin = Path(qt_deploy).resolve().parent
runtime = {path.name.lower(): path for path in qt_bin.glob("*.dll")}
system = Path(os.environ["SystemRoot"]) / "System32"
output = root / "release/windows"
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="NestView-") as temporary:
    stage = Path(temporary) / "NestView"
    stage.mkdir()
    shutil.copy2(executable, stage / executable.name)
    subprocess.run([qt_deploy, "--release", "--compiler-runtime", "--no-translations", "--dir", str(stage), str(stage / executable.name)], check=True)
    todo = list(stage.rglob("*.dll")) + [stage / executable.name]
    visited = set()
    packages = set()
    while todo:
        binary = todo.pop()
        if binary.name.lower() in visited:
            continue
        visited.add(binary.name.lower())
        imports = subprocess.check_output([objdump, "-p", str(binary)], text=True, errors="replace")
        for name in re.findall(r"DLL Name:\s*(\S+)", imports):
            source = runtime.get(name.lower())
            if source:
                target = stage / source.name
                if not target.exists():
                    shutil.copy2(source, target)
                todo.append(target)
                packages.add(source.name)
            elif not (system / name).is_file() and not name.lower().startswith(("api-ms-", "ext-ms-")):
                raise SystemExit(f"Unresolved dependency: {name} ({binary.name})")
    licenses = stage / "licenses"
    licenses.mkdir()
    shutil.copy2(root / "LICENSE", licenses / "NestView-GPL-3.0.txt")
    shutil.copy2(root / "docs/THIRD_PARTY.md", licenses / "THIRD_PARTY.md")
    # Keep the dependency packages' original notices alongside the binaries.
    for directory in (qt_bin.parent / "share/licenses").iterdir():
        if directory.is_dir():
            shutil.copytree(directory, licenses / directory.name, dirs_exist_ok=True)
    (stage / "dependencies.txt").write_text("\n".join(sorted(packages)) + "\n", encoding="utf-8")
    archive = output / "NestView-Windows-x86_64.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                bundle.write(path, path.relative_to(stage.parent))
    # Launch the deployed app with the development DLL directory removed.
    clean_env = {**os.environ, "PATH": str(system), "QT_QPA_PLATFORM": "windows"}
    for key in list(clean_env):
        if key.startswith("QT_PLUGIN") or key == "QT_QPA_PLATFORM_PLUGIN_PATH":
            clean_env.pop(key)
    subprocess.run([str(stage / executable.name), "--smoke-test"], env=clean_env, cwd=stage, check=True, timeout=30)
    print(archive)
