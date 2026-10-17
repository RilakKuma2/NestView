#!/bin/bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"
APP="$PROJECT_DIR/release/NestView.app"
if [[ -e "$APP" ]]; then
  echo "release/NestView.app already exists; move it aside before packaging." >&2
  exit 1
fi
mkdir -p release
/usr/bin/ditto build/NestView.app "$APP"
"$(brew --prefix qtbase)/bin/macdeployqt" "$APP" -always-overwrite
SPARKLE="$PROJECT_DIR/build/_deps/photo_sparkle-src"
/usr/bin/ditto "$SPARKLE/Sparkle.xcframework/macos-arm64_x86_64/Sparkle.framework" "$APP/Contents/Frameworks/Sparkle.framework"
# Do not ship references to the developer's source tree or Homebrew installation.
for rpath in "$(brew --prefix)/lib" "$SPARKLE/Sparkle.xcframework/macos-arm64_x86_64"; do
  if /usr/bin/otool -l "$APP/Contents/MacOS/NestView" | /usr/bin/grep -Fq "path $rpath (offset"; then
    /usr/bin/install_name_tool -delete_rpath "$rpath" "$APP/Contents/MacOS/NestView"
  fi
done
mkdir -p "$APP/Contents/Resources/Licenses"
cp LICENSE "$APP/Contents/Resources/Licenses/qView-GPL-3.0.txt"
cp docs/THIRD_PARTY.md "$APP/Contents/Resources/Licenses/THIRD_PARTY.md"
cp "$SPARKLE/LICENSE" "$APP/Contents/Resources/Licenses/Sparkle.txt"
/usr/bin/codesign --force --deep --sign - "$APP"
/usr/bin/codesign --verify --deep --strict "$APP"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$APP" release/NestView-macOS-AppleSilicon.zip
