# jwpqt

jwpqt is a native Linux port of JWPxp. It continues the recovered JWPce/JWPxp
release history while replacing the Win32 application layer with Qt 6.

The port is intentionally incremental. The historical sources remain at the
repository root as the behavior and format reference. New portable code lives
under `src/core`; Qt-specific application code lives under `src/qt`. See
[`PORTING.md`](PORTING.md) for the extraction boundaries and roadmap.

## Build

On Fedora, install the Qt 6 development package:

```sh
sudo dnf install qt6-qtbase-devel
```

Configure, build, and test out of tree:

```sh
cmake -S . -B /srv/tmp/jwpqt-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /srv/tmp/jwpqt-build
ctest --test-dir /srv/tmp/jwpqt-build --output-on-failure
```

Run the editor, optionally opening a UTF-8 file:

```sh
/srv/tmp/jwpqt-build/src/qt/jwpqt [file]
```

## Current scope

The native editor opens and atomically saves UTF-8, EUC-JP, Shift-JIS,
New/Old/NEC JIS, and JWP B1/B2/J1.20 documents. JWP editing preserves metadata,
paragraph formatting, hard page breaks, code-page interpretation, native
search/replace, and portable transaction history. The portable core also
contains the recovered desktop romaji-to-kana composer plus bounded WNN
dictionary parsing, ordered candidate lookup, a portable user-selection cache,
and atomic preference file I/O. Native conversion sessions, lookup tools,
custom layout/rendering, configuration, and printing remain in progress.

JWPce/JWPxp copyright and licensing notices remain in `_cpright.txt`,
`_readme.txt`, and `gnugpl.txt`.
