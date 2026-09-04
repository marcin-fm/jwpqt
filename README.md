# jwpqt

jwpqt is a native Linux port of JWPxp. It continues the recovered JWPce/JWPxp
release history while replacing the Win32 application layer with Qt 6.

The port is intentionally incremental. The historical sources remain at the
repository root as the behavior and format reference. New portable code lives
under `src/core`; Qt-specific application code lives under `src/qt`.

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

The first native slice provides strict UTF-8 loading and saving plus a basic Qt
text window. Legacy JWP/JIS encodings, the original paragraph model,
kana-to-kanji conversion, dictionaries, lookup tools, and printing have not yet
been ported.

JWPce/JWPxp copyright and licensing notices remain in `_cpright.txt`,
`_readme.txt`, and `gnugpl.txt`.
