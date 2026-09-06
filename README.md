# jwpqt

jwpqt is a native Linux port of JWPxp. It continues the recovered JWPce/JWPxp
release history while replacing the Win32 application layer with Qt 6.

The port is intentionally incremental. The historical sources remain at the
repository root as the behavior and format reference. New portable code lives
under `src/core`; Qt-specific application code lives under `src/qt`. See
[`PORTING.md`](PORTING.md) for the extraction boundaries and roadmap.

## Build

On Fedora, install the native build dependencies:

```sh
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel
```

Configure, build, and test out of tree:

```sh
nice cmake -S . -B /srv/tmp/jwpqt-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
nice cmake --build /srv/tmp/jwpqt-build -j2
nice ctest --test-dir /srv/tmp/jwpqt-build --output-on-failure
```

Run the editor, optionally opening a document:

```sh
/srv/tmp/jwpqt-build/src/qt/jwpqt [file]
```

## Runtime data

`--config-dir` selects application settings and dictionaries; `--user-data-dir`
selects the directory for WNN `user.sel` and `user.cnv`. Without these options,
the native Qt application configuration/data locations are used. Do not change
`XDG_CONFIG_HOME` just to isolate jwpqt: that can also hide the desktop's Qt theme
settings. `user.dct` remains beside `dict.cfg`, unless the registry specifies an
absolute path.

Place `dict.cfg`, its configured dictionaries/indexes, `kanjinfo.dat`,
`radical.dat`, `stroke.dat`, and `radicals.bmp` in the configuration directory.
WNN is automatically loaded when `wnn.dix` or `wnn.dat` is present there; both
are required. `--wnn-data-dir` can select a different WNN directory.

The following Bash example reuses the separately acquired research data on this
development machine. It does not download data, execute Windows programs, or
replace an existing registry or user dictionary. Run it from the repository:

```bash
(
  set -euo pipefail
  data=/srv/tmp/jwpqt-dictionary-research
  profile="${XDG_DATA_HOME:-$HOME/.local/share}/jwpqt-dev"
  config="$profile/config/jwpqt/jwpqt"
  install -d "$config" "$profile/personal"
  install -m 644 \
    "$data"/{wnn.dat,wnn.dix,edict,edict.jdx,classical} \
    "$data"/{kanjinfo.dat,radical.dat,stroke.dat,_cpright.txt} \
    "$data"/jwpce-1.50/{enamdict,enamdict.jdx} \
    radicals.bmp "$config/"
  if [[ ! -e "$config/dict.cfg" ]]; then
    install -m 644 "$data/JWPxp.dic" "$config/dict.cfg"
  fi
  exec /srv/tmp/jwpqt-build/src/qt/jwpqt \
    --config-dir "$config" --user-data-dir "$profile/personal"
)
```

The original `JWPxp.dic` is a supported binary registry and already names
`user.dct`; it does not need to be rewritten as a text file. The dictionaries
have separate licenses from the program: retain `_cpright.txt` and review the
individual notices before redistributing any data.

The clickable **Resources** status and **Help > Runtime Resources** show loaded
sources, failures, and skipped invalid records. Add `--resource-report` to the
launch command to print the same diagnostics and exit; use
`QT_QPA_PLATFORM=offscreen` for a terminal without a display. This report is not
a readiness exit-code check: unavailable optional resources are reported even
when the command succeeds. The recovered classical dictionary loads 647 valid
records with 11 skipped-row warnings; indexed EDICT/ENAMDICT and editable user
dictionaries do not enable malformed-row recovery.

## Current scope

Startup and **File > New Japanese Document** create a native JWP document with
Japanese editing, conversion, formatting, and lookup insertion available without
opening an existing file first. **File > New Text Document** creates unrestricted
Unicode plain text instead; JWP-only tools are unavailable in that mode. Imported
text files currently use this plain-text mode as well.

The native editor opens and atomically saves UTF-8, UTF-7, JFC, EUC-JP, Shift-JIS,
New/Old/NEC JIS, and JWP B1/B2/J1.20 documents. JFC files are plain text:
opening prefers UTF-8, with a strict fallback for the recovered old-EUC
extensions, and saving always writes UTF-8 without a BOM. The `.jfc` extension,
JFC file-dialog filter, or `--encoding jfc` selects that policy.
JWP editing preserves metadata,
paragraph formatting, hard page breaks, code-page interpretation, native
search/replace, and portable transaction history. The portable core also
contains the recovered desktop romaji-to-kana composer plus bounded WNN
dictionary parsing, ordered candidate lookup, a portable user-selection cache,
atomic preference file I/O, candidate-session state, and one-undo document
conversion transactions. With WNN loaded, the native JWP editor converts
selected kana, cycles candidates with Space or Shift+Space, accepts with Enter
or Escape, and loads editable `user.cnv` entries beside learned choices in the
selected user-data directory. Its
default Kanji input mode converts printable desktop romaji to recovered JWP
hiragana and katakana while preserving portable document history. Kana Input
tracks automatic conversion spans, waits for extendable WNN keys, applies
terminal or longest-prefix candidates, and leaves unmatched suffixes at the
caret. JWP paragraph indents, proportional spacing, and hard page breaks now
drive a rich Qt document layout; Format Paragraph edits those values across the
caret paragraph or selected paragraphs with portable undo, and Ctrl+Enter
inserts structural hard page breaks as one undoable command. Global kanji
color policy and `colkanji.lst` are loaded strictly from the application configuration
directory and applied to raw JWP tokens without mutating document formatting;
Kanji Color Options updates the screen policy atomically while preserving
active WNN highlights. Make, Append, Add/Remove, View, and Clear manage the
global color list with atomic persistence. The native modeless EDICT tool loads
ordered indexed or unindexed resources from `dict.cfg`, searches with the
portable direct, adaptive, wildcard, contingent, and name filters, and inserts
selected rows into JWP with portable undo. Editable EDICT `user.dct` and native
kanji lookup tools are available. Multi-document/project lifecycle, remaining
configuration, complete printing, help, and packaging remain in progress.

### Input modes and shortcuts

**Edit > Input Mode** selects Kanji (`Ctrl+K`), ASCII (`Ctrl+Alt+A`), or JASCII
(`Ctrl+J`). Kanji composes romaji; ASCII inserts characters directly; JASCII
inserts recovered full-width characters with Western comma/period and a dash.
`F4` switches Kanji/ASCII, including JASCII to Kanji. Clicking the status-bar
mode button cycles all three. Switching commits pending kana and accepts the
displayed conversion candidate. Plain Unicode text retains direct input.

`Ctrl+A` remains Select All. Close uses `Ctrl+F4`, leaving `Ctrl+W` for conversion.
SKIP uses `Ctrl+Alt+S` and Spahn-Hadamitzky uses `Ctrl+Alt+H`, avoiding native
Save As and Replace shortcuts. Count Kanji remains `Ctrl+Shift+K`.

JWPce/JWPxp copyright and licensing notices remain in `_cpright.txt`,
`_readme.txt`, and `gnugpl.txt`.
