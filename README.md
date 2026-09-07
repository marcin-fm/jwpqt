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

New and Open create independent tabs with their own text, format, selection,
scroll position and undo history; dictionaries are shared. Opening a path that
is already open activates it without reloading unsaved changes. Close removes
only that tab, and closing the last tab leaves a clean Japanese document.
**Save All** visits every document, including unnamed files, and stops on the
first cancellation or error. **Close All** and Quit check every document before
discarding any. **Window > Next/Previous File** (`Ctrl+Tab`/`Ctrl+Shift+Tab`, also
`Ctrl+PageDown`/`Ctrl+PageUp`) and **Window > Files** (`Alt+W`) switch documents.
Modeless dictionary and character-information windows insert into the currently
active tab, even after their original document is closed. Save As and Export
Copy cannot overwrite another open document's path.
**Count Kanji** refreshes the current/all-document counts on every Count,
including JIS kanji in unrestricted Unicode documents without changing their
text or history. **View Kanji Color List** opens a separate Japanese tab.

**File > Recent Files** retains nine documents or projects, including each text
encoding or JWP code page. Reopening activates an existing tab or opens a new
one without guessing the encoding again. History is stored atomically in
`recent-files.json` in the application configuration directory. Failed opens,
cancelled dialogs, and Export Copy do not add entries. **Clear Recent Files**
clears history, not documents. Corrupt history is preserved until an explicit
Clear; history errors appear in Runtime Resources without failing document I/O.
Automatic history writes pause while that history file is open as a document.

**File > Open Project** opens current Unicode `.jpr` workspaces, with a choice
to replace or append to the open tabs. Files and supported settings are validated
before replacement; save/discard/cancel decisions are collected before any
editor is removed. Files are reread after save consent, including a changed
encoding. Appending retains already-open buffers and their formats, not stale
disk copies. Windows drive/UNC paths ask for equivalent Linux directories;
ambiguous legacy text asks for its encoding rather than assuming UTF-8.

**File > Save Project** offers to save the documents or just their references.
References do not contain unsaved text or undo history; save nonempty unnamed
documents first. A reference-only save does not accept an active conversion.
Projects preserve explicit file formats, code pages, editing preferences,
supported settings and native tab order. A bounded configuration comment carries
native metadata while the legacy path list keeps the active file last. Unsupported
settings are retained and require approval when opening; they are not silently
treated as implemented. Restoration is bounded to 1024 documents, 64 MiB of
source bytes and 33,554,432 Qt character positions; nested projects are rejected.

`jwpqt workspace.jpr` opens a project automatically. Use
`jwpqt --project --encoding old-jis legacy.jpr` to supply an encoding fallback
for ambiguous legacy references; explicit native metadata still takes priority.
Windows-directory choices and approval of unapplied settings require the GUI
or the explicit project API options. Recent project entries reopen as projects.

**Tools > Options** configures Japanese document/query/list/candidate fonts,
font inheritance, toolbar/status/candidate visibility, scrollbar policies,
candidate-bar position, default JWP code page and exit/history persistence.
Desktop menu fonts are unchanged. Character Table stays at 16 logical pixels;
the large character keeps its default glyph size. Unavailable or legacy bitmap
font families use native fallbacks, with their original names retained.
**Default Settings**, **Save Settings** and **Import Settings** operate on this
supported subset. Imports overlay known values and retain unsupported entries
from the imported file; Default Settings retains existing unsupported entries.
Runtime Resources and the Options dialog disclose retained, unapplied settings.
Preferences load from `jwpqt.cfg` in the application configuration directory;
Save and the optional save-on-exit use atomic replacement. Invalid settings are
not applied, corrupt startup files are not automatically overwritten, and saving
cannot overwrite an open document. Font changes preserve every tab's text,
history and selection, including an active conversion preview.

Startup and **File > New Japanese Document** create a native JWP document with
Japanese editing, conversion, formatting, and lookup insertion available without
opening an existing file first. **File > New Text Document** creates unrestricted
Unicode plain text instead; JWP paragraph formatting and WNN conversion are
unavailable in that mode, but dictionary and character lookup/insertion work. Imported
text files use Japanese editing when all their characters are representable in
JWP, but keep their text encoding and file format. Other Unicode stays intact in
unrestricted editing. **Edit > Input Mode > Japanese Editing** switches engines
without changing the saved format; it asks before clearing Undo/Redo or dropping
JWP layout/metadata. A switch into Japanese editing rejects unsupported characters
rather than substituting them.

**Save As** offers JWP and every supported text format regardless of editing
engine. Text output asks before losing paragraph/page layout, headers, footers,
summary metadata, or hard page breaks. Unsupported characters fail before the
destination is changed. **File > Export Copy** writes another format without
changing the current path, saved baseline, dirty state, or undo history; finish
pending input/conversion first. A copy cannot overwrite its own source.

The native editor opens and atomically saves UTF-8, UTF-7, UTF-16LE/BE, JFC,
EUC-JP, Shift-JIS, New/Old/NEC JIS, and JWP B1/B2/J1.20 documents. JFC files are plain text:
opening prefers UTF-8, with a strict fallback for the recovered old-EUC
extensions, and saving always writes UTF-8 without a BOM. The `.jfc` extension,
JFC file-dialog filter, or `--encoding jfc` selects that policy.
UTF-16 preserves byte order and the input BOM, supports supplementary Unicode,
and rejects malformed surrogate pairs. BOM-marked files are detected; unmarked
files require their explicit filter or `--encoding utf-16le` / `utf-16be`.
Switching to UTF-16 through the encoding menu writes a BOM for detection.
JWP editing preserves metadata,
paragraph formatting, hard page breaks, code-page interpretation, native
search/replace, and portable transaction history. The portable core also
contains the recovered desktop romaji-to-kana composer plus bounded WNN
dictionary parsing, ordered candidate lookup, a portable user-selection cache,
atomic preference file I/O, candidate-session state, and one-undo document
conversion transactions.

**Convert** also replays an ASCII selection in either editing engine without
requiring WNN data. Lowercase romaji becomes hiragana, uppercase syllables produce
katakana, and capitalized spans can use the preferred loaded WNN candidates.
The result remains selected in the original direction; one Undo restores the
entire original selection. In Japanese editing, a second Convert can then start
ordinary kana-to-kanji candidate selection. Replay is staged before replacement:
incomplete romaji, unsupported input, or an exceeded bound leaves the source
unchanged. The selection must stay within one paragraph and contain only printable
ASCII or tabs, with at most 65,535 input/output cells. Finish pending kana input
before replaying; overwrite mode never consumes text beyond the selected range.

In Japanese editing with WNN loaded, Convert handles selected kana or a
pending automatic conversion range without prematurely accepting the preview.
The horizontal strip below the editor previews candidates on click; Space,
Shift+Space or Convert cycles them, and Enter or Escape accepts the displayed
candidate as one undoable conversion. Editable `user.cnv` entries live beside
learned choices in the selected user-data directory. Its
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
kanji lookup tools and project workspaces are available. Remaining configuration,
input/lookup controls, complete printing, help, and packaging remain in progress.

### Input modes and shortcuts

**Edit > Input Mode** selects Kanji (`Ctrl+K`), ASCII (`Ctrl+Alt+A`), or JASCII
(`Ctrl+J`). Kanji composes romaji; ASCII inserts characters directly; JASCII
inserts recovered full-width characters with Western comma/period and a dash.
`F4` switches Kanji/ASCII, including JASCII to Kanji. Clicking the status-bar
mode button cycles all three. Switching commits pending kana and accepts the
displayed conversion candidate. Plain Unicode text retains direct input.

`Insert`, **Edit > Input Mode > Overwrite Mode**, or the status-bar `INS`/`OVR`
button switches document typing between insert and overwrite. The mode is shared
by this window's tabs, including newly opened/project-restored documents, but is
not saved in settings or projects. Toggling does not commit pending kana or a
conversion preview. ASCII, composed kana/JASCII and ordinary IME commits honor
the mode with one-step undo; explicit IME replacement ranges remain authoritative.
Overwrite replaces complete Unicode scalars without consuming a paragraph break.
Typing over a selection replaces only the selection, deliberately avoiding the
legacy behavior that also overwrote following text. Clipboard and lookup insertion
remain insert operations: `Ctrl+Insert` copies, while `Shift+Insert` and
`Ctrl+Shift+Insert` paste.

Dictionary and Reading Lookup query fields share the window's insert/overwrite
mode while retaining their own K/A/J input modes. `Insert` in a query updates the
shared mode without flushing pending kana; copy/paste shortcuts stay in that query,
not the document behind it. Typed kana/JASCII, Unicode and ordinary IME commits
use scalar-safe, selection-only replacement and native query undo. Mode hints
are available in the query tooltips and accessibility descriptions. Invalid input,
field limits, validators and read-only targets cannot erase text before rejection.
Other edit forms, remaining clipboard formats and exact legacy visual-line-edge
behavior remain separate input-parity work.

`Ctrl+A` remains Select All. Close uses `Ctrl+F4`, leaving `Ctrl+W` for conversion.
SKIP uses `Ctrl+Alt+S` and Spahn-Hadamitzky uses `Ctrl+Alt+H`, avoiding native
Save As and Replace shortcuts. Index Lookup uses `Ctrl+Shift+I`; Count Kanji
remains `Ctrl+Shift+K`. Japanese editor, candidate and lookup content defaults
to the recovered 16-logical-pixel size, independently of the desktop UI font.

### Menus and toolbar

The toolbar is visible by default and shares the menu actions for files,
clipboard/history, search, input modes, conversion, dictionaries, kanji tools,
and page layout. **View > Toolbar** hides or restores it; narrow windows expose
the remaining actions through the toolbar's extension button. Mode checks and
disabled actions track the editor and loaded resources.

Menu text and the embedded legacy Japanese toolbar artwork adapt to light/dark
palettes, including mismatched desktop text colors. Low-contrast monochrome
standard icons also adapt, while colored artwork is retained. The 29 toolbar
actions include Index Lookup; short text or native icon fallbacks remain when
no icon theme is installed. Toolbar visibility is saved with native preferences;
customization and saved placement remain open. Options is available in Tools.

### Dictionary and kanji lookup

Dictionary Search replaces results in the same window without opening or
raising the separate result history. Headwords/readings and definitions are
selectable text; right-clicking an exact character opens its context menu and
independent Character Information window. Copy uses the selected display text;
Insert uses the full dictionary entries touched by that selection.

Dictionary, user-conversion, character-table and kanji lookup results can be
inserted into either editing engine. They replace the active document's current
selection as one undo step, preserving its encoding, Unicode characters and
saved-state tracking. Modeless windows follow the active tab, even after project
restoration. Insertion refuses active conversion, read-only targets, invalid
Unicode and selections that split a surrogate pair rather than damaging text.

Dictionary and Reading query fields have their own **K / A / J** mode button
and `F4` toggle. Kanji mode composes romaji into kana; ASCII and JASCII work
without changing the main editor's mode. Search resolves pending kana once.

Dictionary **Begin With** and **End With** control word boundaries; Begin With
starts enabled, matching the legacy default. **Full ASCII** applies those
boundaries to complete definitions, and **JASCII to ASCII** explicitly normalizes
full-width query text. **Advanced** enables adaptive deinflection, independently
of wildcard syntax, with **Always Search**, **Show All** and **I-adjectives**
controls. These choices survive closing the lookup or replacing its resources
within the same application window. Their cross-session settings migration is
still pending. Changing a checkbox does not discard results or start a search.

Character Table places its code fields beside the full character grid. Lookup
results use horizontal character strips with Information, Insert and Copy
actions. Bushu, Stroke/Bushu, SKIP, Spahn-Hadamitzky, Four Corner and Index Lookup
share a tabbed window. Graphical radical selectors, recovered SKIP/Four Corner
legends, Clear and debounced Auto Search are available where applicable. Index
Lookup supports the 21 recovered dictionary/reference types, limited by the
loaded metadata; its volume field is enabled only for the relevant indexes.
Additional selector/legend artwork is embedded, requiring no new provisioning.
In dark palettes, radical strokes use light ink on dark paper and stroke-count
headings stay readable. Switching back restores the original light artwork
without changing queries, selections or results.

This is not complete dialog parity: dictionary query history, sort/options
integration, radical automatic stroke estimation and stroke-tolerance shortcuts
remain open. Existing numeric ranges
and portable dictionary search algorithms do not substitute for those controls.

### Character information

Right-click a character and choose **Character Information**, or use `Ctrl+I`
at the caret/selection. Shift-right-click opens information directly. Each
request opens an independent modeless window, including requests from the
readings or **More Info** panes; the original windows remain open.

The viewer shows a large glyph, character codes, kanji metadata, and separate
meanings/on-yomi/kun-yomi/nanori sections. Kana shows its supported romaji
spellings. Basic character codes also work without `kanjinfo.dat`, including
ASCII and Unicode text outside the JWP character set.

**From Clipboard** changes only its own window. **Insert to File** inserts
selected reading text; double-clicking the large glyph inserts that character.
Both use the editor's normal history. Inspecting characters does not change
the editor's selection or commit an active conversion. Closing one information
window does not close the others.

**Tools > Character Info Setup**, also available inside Options, configures the
26 legacy information fields, their order and blank spacers. Duplicate choices
are repaired using the first missing field, as in the original setup dialog.
The scrollable main panel shows all configured rows; rows 14-26 also appear in
More Info. Bushu and cross-references remain available independently. Compact
readings use comma-space for meanings and Japanese commas for kana; headings
can be hidden. Accepted changes update existing and new information windows.
Cancel, including cancellation of the enclosing Options dialog, leaves the
effective settings unchanged.

These preferences persist in `jwpqt.cfg` and JPR workspaces. The full 60-byte
legacy field array is preserved, including the unused tail; Setup defaults reset
only the visible choices and flags. `CharInfo_SingleDialog` is retained but not
applied: native information windows remain independent.

JWPce/JWPxp copyright and licensing notices remain in `_cpright.txt`,
`_readme.txt`, and `gnugpl.txt`.
