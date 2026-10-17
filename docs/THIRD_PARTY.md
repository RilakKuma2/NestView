# Third-party software

NestView is a modified fork of qView 7.0, licensed under GPL-3.0.

- qView: https://github.com/jurplel/qView
- Source baseline: c5eca1c7176549e0f0718d11201547ddcdb1f8c9
- qView license: the repository's LICENSE file, retained unchanged.
- Qt 6.11.2: dynamically linked. https://www.qt.io/licensing/open-source-lgpl-obligations
  Source and license texts: https://download.qt.io/archive/qt/6.11/6.11.2/
- libarchive 3.8.9: BSD license. https://github.com/libarchive/libarchive/blob/master/COPYING
- Additional codec libraries packaged by Qt/macdeployqt retain their upstream licenses.
  The exact bundled dependency paths are recorded in release/dependencies.txt.
- macOS Cocoa, ImageIO, and CoreGraphics are system frameworks.
- qView's fonts and existing assets retain their original notices and upstream attribution.

- Sparkle 2.10.0: https://github.com/sparkle-project/Sparkle
  The distribution's full license notices are included in the app at
  Contents/Resources/Licenses/Sparkle.txt.

NestView's added line icons and mark were drawn in code for this project.

- Windows/Linux: Exiv2 (GPL-2.0-or-later), https://exiv2.org/ .
- Windows/Linux: LibRaw (LGPL-2.1 or CDDL-1.0), https://www.libraw.org/ .
- Windows builds include the dependencies' license notices in the ZIP's `licenses` directory. Linux packages depend on the system-provided libraries and license notices.
