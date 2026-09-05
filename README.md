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
atomic preference file I/O, candidate-session state, and one-undo document
conversion transactions. With `--wnn-data-dir`, the native JWP editor converts
selected kana, cycles candidates with Space or Shift+Space, accepts with Enter
or Escape, and loads editable `user.cnv` entries beside learned choices in the
XDG user-data directory. Its
explicit Kana Input mode converts printable desktop romaji to recovered JWP
hiragana and katakana while preserving portable document history. Kana Input
tracks automatic conversion spans, waits for extendable WNN keys, applies
terminal or longest-prefix candidates, and leaves unmatched suffixes at the
caret. JWP paragraph indents, proportional spacing, and hard page breaks now
drive a rich Qt document layout; Format Paragraph edits those values across the
caret paragraph or selected paragraphs with portable undo, and Ctrl+Enter
inserts structural hard page breaks as one undoable command. Global kanji
color policy and `colkanji.lst` are loaded strictly from the XDG configuration
directory and applied to raw JWP tokens without mutating document formatting;
Kanji Color Options updates the screen policy atomically while preserving
active WNN highlights. Make, Append, Add/Remove, View, and Clear manage the
global color list with atomic persistence. User dictionary editing, lookup
tools, remaining configuration, and printing remain in progress.

JWPce/JWPxp copyright and licensing notices remain in `_cpright.txt`,
`_readme.txt`, and `gnugpl.txt`.
