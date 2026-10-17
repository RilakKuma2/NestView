# Desktop builds

CMake 3.21+, a C++17 compiler and Qt 6.4+ are required. Qt Core, Gui, Widgets, Network, Svg, PrintSupport, Concurrent and Test are used. Libarchive is required on all platforms. Windows and Linux additionally use Exiv2 for metadata and LibRaw for RAW images. Platform packages retain their original open-source licenses.

## Windows x86_64

Install [MSYS2](https://www.msys2.org/) and open a **UCRT64** terminal:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,python,qt6-base,qt6-svg,qt6-tools,qt6-imageformats,exiv2,libraw,libarchive}
bash scripts/build-windows.sh
```

The script builds and runs tests, deploys Qt's Windows and image plugins, recursively collects runtime DLLs and verifies that the packaged executable starts without the development environment in PATH. Output: `release/windows/NestView-Windows-x86_64.zip`. Extract the entire ZIP before running `NestView.exe`. Windows code signing is not configured, so SmartScreen may show a warning.

## Ubuntu 24.04 / compatible distributions

```sh
sudo apt-get install cmake ninja-build g++ qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools qt6-image-formats-plugins qt6-wayland libarchive-dev libexiv2-dev libraw-dev libx11-dev desktop-file-utils
bash scripts/build-linux.sh
sudo apt install ./release/linux/NestView-Linux-x86_64.deb
```

The DEB declares runtime dependencies using `dpkg-shlibdeps`. It uses system Qt and codec libraries. Other Linux distributions can build with the equivalent packages and install using `cmake --install build-linux --prefix /usr/local`.

Both X11 and Wayland use Qt's native platform plugin. X11 monitor profiles are read when available; Wayland currently falls back to the image's own color space or the user's explicit sRGB/Display P3 setting. Wallpaper changes use the desktop portal when provided by the desktop environment. `-DNO_X11=ON` disables the optional X11 profile integration.

## macOS

The existing `scripts/build-macos.sh` pipeline uses Cocoa/ImageIO and Sparkle. It is independent of Windows/Linux dependencies.

To test the portable backends on macOS without changing the release build:

```sh
brew install exiv2 libraw
cmake -S . -B build-portable -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix);$(brew --prefix libarchive)" -DBUILD_TESTS=ON -DNEST_PORTABLE_MEDIA=ON -DNEST_PORTABLE_UPDATER=ON
cmake --build build-portable
ctest --test-dir build-portable --output-on-failure
```

## Updates and artifacts

The desktop workflow produces Windows ZIP and Linux DEB artifacts after tests and package checks pass. To distribute them as an update, attach the same filenames to a stable GitHub Release with a newer version tag. Portable builds check only this project's GitHub repository and open the matching installer in the browser. They do not execute downloaded installers or replace running binaries automatically. macOS continues to use the signed Sparkle feed.
