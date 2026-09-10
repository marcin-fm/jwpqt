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
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel poppler-utils desktop-file-utils shared-mime-info
```

Configure, build, and test out of tree:

```sh
nice cmake -S . -B /srv/tmp/jwpqt-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
nice cmake --build /srv/tmp/jwpqt-build -j2
nice ctest --test-dir /srv/tmp/jwpqt-build --output-on-failure
```

The printing tests require `pdftotext` from `poppler-utils` to inspect actual PDF
page content, not just file signatures. It is not an application runtime dependency.
Installed-delivery tests also require `desktop-file-validate`, `update-mime-database`
and CPack. They stage and extract packages under the build directory, without root
installation or access to the user's configuration.

Run the editor, optionally opening documents and projects in order:

```sh
/srv/tmp/jwpqt-build/src/qt/jwpqt [file...]
```

An unreadable argument is reported without preventing later arguments from
opening. Repeated paths activate the existing tab instead of creating hidden
duplicates. If every supplied path fails, noninteractive startup exits with a
failure status. Each diagnostic belongs to that argument; a project error is
never reused for a later ordinary file.

## Help And Installation

Help > Contents opens the embedded offline handbook. Search matches topic titles
and text; Back/Forward, Contents and zoom controls support navigation. F1 routes
the owning editor/dialog to its relevant topic. About includes original credits,
GPL terms and data notices, and About Qt identifies the system toolkit. Help
reuses its own window without changing document text or undo. Only whitelisted
embedded pages and resources can load, never arbitrary local files or websites.
`jwpqt --handbook` opens the same handbook at startup without optional datasets.

Build an installable release and matching source archive:

```sh
nice cmake -S . -B /srv/tmp/jwpqt-build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
nice cmake --build /srv/tmp/jwpqt-build/release -j2
nice cpack --config /srv/tmp/jwpqt-build/release/CPackConfig.cmake -B /srv/tmp/jwpqt-build/packages
nice cpack --config /srv/tmp/jwpqt-build/release/CPackSourceConfig.cmake -B /srv/tmp/jwpqt-build/packages
```

The binary TGZ contains `bin/jwpqt`, the handbook, retained licenses, a desktop
entry, application icon and JWP/JPR MIME definitions. Extract it into a private
prefix and run its executable from any working directory. CMake also supports
`cmake --install BUILD --prefix PREFIX` and DESTDIR staging. Desktop registration
does not force a default application; put the executable on PATH and refresh
desktop/MIME caches using the distribution's normal tools. Removing a private
prefix must not remove separate user configuration or documents.

This is a dynamically linked Linux package, not an AppImage or a universal
binary. It requires a compatible architecture, system Qt 6 Widgets/PrintSupport,
standard C++ libraries and Japanese fonts. Supply matching source and retained
notices when redistributing binaries under the GPL. Source archives omit Git and
local assistant metadata. Neither package adds optional dictionary corpora;
their separate acquisition and license conditions still apply. The installed
handbook explains paths, provisioning and uninstallation.

## Runtime data

`--config-dir` selects application settings and dictionaries; `--user-data-dir`
selects the directory for WNN `user.sel` and `user.cnv`. Without these options,
the native Qt application configuration/data locations are used. Do not change
`XDG_CONFIG_HOME` just to isolate jwpqt: that can also hide the desktop's Qt theme
settings. `user.dct` remains beside `dict.cfg`, unless the registry specifies an
absolute path.

JWPxp's first-argument `+directory`/`-directory` network syntax split Win32
configuration paths and could suppress resource errors. It is not accepted as
an ambiguous positional alias. Use the explicit native directory options above;
resource failures remain visible in Runtime Resources.

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

**Tools > Manage Dictionaries** (also available inside lookup) provides staged
add/edit/remove/reorder, search flags, EUC/UTF-8/mixed and name/classical roles,
index/keep/quiet preferences, defaults and bounded inspection. **Save and Reload**
validates resources before checked atomic registry replacement. Unavailable
searched resources require explicit acceptance, and a stale disk snapshot cannot
be overwritten. Lookup queries/results/history remain intact; close the user
dictionary editor first to protect its working copy. ANSI registry migration
asks for the original code page and writes Unicode only on successful Save.
Detect asks before replacing an existing description and defaults to retaining
it; declining still applies the staged path, encoding and companion-index data.
Quitting with unsaved WNN or EDICT user-dictionary edits offers Save, Discard
or Cancel before document and settings checks. Discard applies only to that
close attempt, so a later cancellation leaves the working copy dirty.
Closing either editor from its title bar separately asks whether to save, with
Save as the safe default; No closes that working copy without publishing it.
The editor's explicit Cancel button remains the intentional discard command.
User Conversions warns before its additive Import and defaults to No. Both
editors start Import in the directory of their configured user file, then parse
every selected file before appending anything to the working copy.
The [handbook](docs/handbook/dictionary.md) describes the complete native workflow.

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
only that tab. Closing the last tab asks whether to exit; No leaves a clean
Japanese document, without restoring edits already discarded during Close.
Options can disable this confirmation or make the window close button close
only the current file. Alt-close forces application exit, Ctrl-close forces file
close, and explicit Quit always exits the application after the save checks.
These policies persist through settings and projects. **Open dictionary on
startup** applies only when no explicit document/project is supplied; unavailable
dictionary resources are reported. Resource-report mode never opens startup UI.

**Restore main window** persists normal bounds and maximized state without recording
minimized geometry. Six source dialog roles independently remember their placement;
existing windows are not relocated by importing settings. New placements are bounded
and clamped to available native screens, and independent information viewers remain
independent. Geometry is included in settings/JPR.

**Restore named files from the previous session** is opt-in. It reads app-scoped
`last-session.jpr` before command-line files, preserving saved formats and active
tab order while keeping current preferences. Missing or invalid references are
reported; usable files still open. Named files are recorded after exit save/discard
checks, independently of query-history saving. Unnamed buffers and unsaved text
are never serialized. The next successful save records currently open named files,
so skipped references are omitted. Disabling restoration leaves the archive unchanged.
Corrupt, locked or externally changed archives are not overwritten; failed exit saving
offers Cancel or exit without saving. The archive is not added to project history.
Resource-report reads its status without reopening the workspace. This is native
typed storage, not automatic import of the Windows history file's path tail.
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
candidate-bar position, default JWP code page, dictionary search policies and
exit/recent-file/query-history persistence and history capacity.
Desktop menu fonts are unchanged. Character Table stays at 16 logical pixels;
the large character fits its available pane as the window is resized. Unavailable
font families inherit their parent role's family and size, with their original
names retained and fallback diagnostics shown. Legacy bitmap font files are not
silently treated as loaded native fonts.
**Default Settings**, **Save Settings** and **Import Settings** operate on this
supported subset. Imports overlay known values and retain unsupported entries
from the imported file; Default Settings retains existing unsupported entries.
Runtime Resources and the Options dialog disclose retained, unapplied settings.
Preferences load from `jwpqt.cfg` in the application configuration directory;
Save and the optional save-on-exit use atomic replacement. Invalid settings are
not applied, corrupt startup files are not automatically overwritten, and saving
cannot overwrite an open document. Font changes preserve every tab's text,
history and selection, including an active conversion preview.

The **Bitmap** font role controls the image representation of copied document
text, inheriting File when automatic. Copy/Cut retain plain text (including
nonbreaking spaces and Unicode signatures) and Qt's rich formats, and add a
white-background native text image. **Clipboard_Omit_Bitmap** disables only the
image. Rendering uses a separate document, bounded at 262,144 selected UTF-16
positions, 8,192 pixels per edge and 16 million pixels; oversized images are
omitted with a status message while text remains available. **Bitmap.Vert**
counter-rotates Japanese glyphs using the same source rules as PDF printing;
Latin stays horizontal; raster fonts retain the source punctuation exceptions. It uses the
bitmap font's own settings even when Automatic is retained. This is the legacy
clockwise-paper-turn convention, not a rotation of the entire image.
**ColorKanji_Clipboard** applies persistent kanji foregrounds only while list
coloring is active. Selection and conversion highlights are never copied into
the image. These options persist through settings, restart and JPR; original
raster punctuation and small-kana positioning uses the source ink-bound rules.
The raster header's extra-character count does not select substitutions in the
source; standard glyph availability follows the physical table. Non-square
raster rotation is rejected rather than reading outside the bitmap. TrueType
fonts with a `vert` GSUB feature use its first lookup/subtable and rotate all
Japanese glyphs, including the substituted punctuation. Bounded private native
faces retain original Unicode in PDF extraction, outlines, style and licensing
metadata without changing horizontal fallback or exposing generated font names.
Missing `vert` keeps the native fallback; malformed selected data fails explicitly.
With no selection, the recovered Ctrl+C shortcut copies the current visual line
and restores the cursor; Ctrl+X cuts that line as one undoable edit. Empty lines
and read-only Cut are no-ops. Selected Copy/Cut and every clipboard format keep
their ordinary native paths.

**Options > Fonts > ASCII and legacy extensions** selects an independent
single-byte font at the Japanese role's height, without changing desktop menus
or document formatting. A private character-restricted face prevents a font
that also contains CJK from taking over Japanese glyphs. An unavailable family
uses the native default with a diagnostic; original names persist through
settings and projects. Source `ASCII.Size` and `ASCII.Auto` are retained but do
not override the matched height. Printing and clipboard images use the same
selection. Optional Latin ligatures are disabled for source-style individual
characters and reliable PDF text. Original outlines, copyright/license records
and embedding flags are preserved; no font is installed globally or bundled.
Private copies are bounded at 16 faces and 64 MiB per application cache.
Native documents retain the original byte/JIS distinction as display-only
metadata, even when both encode identical Unicode. Raw bytes use the ASCII face
and remain upright; JIS characters use the Japanese face and its vertical rules.
Clipboard images and PDF bodies/headers preserve that identity without changing
text, native tokens or undo. Unrestricted Unicode has no invented byte identity.

Legacy **`.f00` font files** can be entered instead of a font family, including
the separate Print role. Relative paths resolve from the application settings
directory; absolute paths are accepted. Native roles use the bitmap's original
height (Table remains 16 px), Big fits its pane, and Print retains its physical
point size. The portable decoder supports packed and holey JIS tables and both
byte- and word-aligned rows, with safe fallback for missing final JIS slots.
Original ink pixels become bounded rectangle outlines in private in-memory Qt
faces, not a replacement Win32 layer or guessed modern typeface. Private face
identities are not offered as persistent font names. Loading is bounded at 8 MiB
per source, 64 MiB per generated face and 64 MiB/16 faces per application cache.
The source font files are never modified, installed globally or bundled; their
original licensing still applies, and generated faces restrict font embedding.

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

**Save As** offers Normal JWPce (`.jce`), JWP (`.jwp`), and every supported
text format regardless of editing engine. Text output asks before losing
paragraph/page layout, headers, footers,
summary metadata, or hard page breaks. Unsupported characters fail before the
destination is changed. **File > Export Copy** writes another format without
changing the current path, saved baseline, dirty state, or undo history; finish
pending input/conversion first. A copy cannot overwrite its own source.

**Options > Keep the previous disk version** enables source-compatible `_BAK`
backups (off by default). A successful overwrite copies the exact previous disk
bytes to the appended name, e.g. `notes.jwp_BAK`, before atomically publishing
the new document. Invalid encoding or failed backup creation leaves the document
and old backup intact. Backup symlinks, open documents and protected application
data are rejected; new files do not remove an existing backup. This is not a
two-file transaction: if final document publication fails after backup succeeds,
the backup contains the still-current disk version. Backup copying uses bounded
memory and retains file permissions, not all filesystem-specific metadata.

The native editor opens and atomically saves UTF-8, UTF-7, UTF-16LE/BE, JFC,
EUC-JP, Shift-JIS, New/Old/NEC JIS, and JWP B1/B2/J1.20 documents. The source's
`.jce` and `.jwp` choices use the same structured JWP codec; `.jce` is the
default for a new Japanese document, while an existing extension is retained.
Interactive Open can recover the readable paragraph prefix of a damaged JWP
document after its header and metadata have validated. The warning defaults to
No and reports what was retained; an accepted recovery is marked modified, and
the damaged source is not rewritten until an explicit save. Automated opens,
project staging and Revert remain strict. Valid packed JWPxp x86 undo payloads
are skipped because the source loader did not restore them. Malformed or
truncated payloads, bad metadata and safety-limit failures remain fatal; only
the recovered x86 layout is recognized. Native saves always omit embedded undo.
JFC files are plain text:
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
caret paragraph or selected paragraphs with portable undo. Invalid hanging
indents or page-width combinations are reported without closing the editor, so
the retained values can be corrected in place. Ctrl+Enter
inserts structural hard page breaks as one undoable command. Global kanji
color policy and `colkanji.lst` are loaded strictly from the application configuration
directory and applied to raw JWP tokens without mutating document formatting;
Kanji Color Options updates the screen policy atomically while preserving
active WNN highlights. Make, Append, Add/Remove, View, and Clear manage the
global color list with atomic persistence. Options > Colors also imports source
COLORREF values, list mode and uncommon-color policy through settings and JPR.
Absent fields inherit the existing native INI color store; explicit fields win,
and Inherit restores that store without restarting. Information heading colors
adapt for theme contrast. Optional uncommon-character dots decorate conversion
and lookup bars without changing their text, Copy or Insert. Special palette
references remain preserved and disclosed. The same readable highlight color marks
classical/user dictionary entries and Advanced labels, while priority/contingent
labels remain ordinary. Source provenance survives sorting and first-record
deduplication; accumulated results retain it too. Live preference/theme changes
preserve result text, selection, canonical insertion and document undo.

The native modeless EDICT tool loads
ordered indexed or unindexed resources from `dict.cfg`, searches with the
portable direct, adaptive, wildcard, contingent, and name filters, and inserts
selected rows into JWP with portable undo. Editable EDICT `user.dct` and native
kanji lookup tools and project workspaces are available. Native document printing
and preview are described below. The recovered main-menu command inventory and
configuration surface are covered by native implementations or explicit platform
replacements and exclusions.

### Document printing and preview

**File > Print** and **Print Preview** use a frozen document,
formatting, selection and metadata snapshot. Editing or switching documents while
a dialog is open cannot substitute another document into the job. Preview offers
page navigation, zoom and Print through the same protected output path; preview
alone never writes to the remembered PDF destination.

The native renderer paginates in physical points, preserving paragraph indents,
spacing, hard page breaks and hanging indents. Print supports exact selected text,
contiguous or disjoint page ranges, reverse order, collated/uncollated copies and
cancellation. Selected/ranged PDF output does not merely hide unwanted text behind
a clip: it omits that text from the PDF stream. Printer Setup applies accepted JWP
margins and orientation as one undoable change, only to its original document.

Options > Default Page persists source-compatible default margins and orientation
for new Japanese documents and text imports. Page Layout provides From Default
and Set as Default, staged until acceptance; Cancel preserves both document and
preferences. Existing JWP files keep their saved layout. The legacy 20-byte
little-endian record is validated explicitly and retains its two padding bytes.
Unchanged fractional margins are not silently rounded by the dialog.

JWP odd/even headers and footers support first-page suppression and the source
`&A`, `&C`, `&D`, `&F`, `&K`, `&L`, `&N`, `&S`, `&T`, `&P` and `&&` substitutions.
Date/time and output policies are captured once for the job. **Options > Printing**
configures the source date/time/AM/PM patterns and header offsets. Defaults are
`&y/&M/&D` and `&h:&N &A`; the legacy rules intentionally use AM at noon and
hour zero at midnight. Date/time allow 19 JWP characters and AM/PM text allows 9;
original unused configuration-array cells remain preserved.
Vertical JWP output preserves the source paper convention: Japanese glyphs are
counter-rotated while Latin and the recovered exception punctuation stay on the
horizontal baseline, for reading the paper after a clockwise quarter turn. Native
font shaping is retained; this is not a new top-to-bottom writing mode or a promise
of pixel-identical legacy bitmap/vertical-GSUB placement.

**Options > Printing** stores `Print.Font`, `Print.Size` and `Print.Auto` through
configuration and projects. The size is physical points in the dialog and tenths
of a point on disk, independently of screen font sizes. Automatic uses the document
font family; explicit families use Qt's native font fallback. **ColorKanji_Printing**
enables kanji colors only with an active list mode, including header/footer text;
list colors retain precedence over uncommon colors. Neither later preference
changes nor transient editor highlighting alter a captured print job.
**Printing_Justify_ASCII** (default on) applies the source's next-cell padding
before tabs and on eligible wrapped lines, not at final paragraph ends or before
JIS text. Native JWP output uses fixed Japanese cells and one-cell tab stops even
when ASCII padding is off. A bounded grid-aware line-fitting pass keeps narrow JIS
glyphs inside the paragraph while retaining Qt word boundaries, styles, selection,
hard page breaks and the original text. Unicode-only documents keep Qt layout.

PDF output is completed in a temporary file beside its destination and published
atomically. Invalid geometry/ranges, cancellation (including the final checkpoint)
or a print destination opened as a document during progress cannot destroy a prior
file. Settings, histories and the current project are also protected destinations.
Paper size, page/format/run counts, copies and rendering work are bounded; oversized
jobs report an error rather than silently truncating. Native spool errors are
reported, but cancellation cannot recall pages already sent to a physical printer.
PDF text/image/preview verification is the accepted printing gate for this port;
physical-printer testing is not required. This does not claim testing of every
printer backend or pixel-identical Win32 rasterization.

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
The remaining Japanese-input forms and native/external clipboard representations
use the same validated composition, overwrite and one-step undo boundaries.

`Ctrl+A` remains Select All. Close uses `Ctrl+F4`, leaving `Ctrl+W` for conversion.
SKIP uses `Ctrl+Alt+S` and Spahn-Hadamitzky uses `Ctrl+Alt+H`, avoiding native
Save As and Replace shortcuts. Index Lookup uses `Ctrl+Shift+I`; Count Kanji
remains `Ctrl+Shift+K`. Japanese editor, candidate and lookup content defaults
to the recovered 16-logical-pixel size, independently of the desktop UI font.
Right-clicking a visible conversion candidate, or Alt-clicking it, selects that
candidate and opens an independent Character Information window for its first
character without accepting the conversion.
Non-conflicting recovered aliases are also available, including `F7`/`F8`/`F9`
for previous/find/next, `Shift+F8` for Replace and the original Alt-letter file
commands. Conflicting source bindings such as `Ctrl+S` for Find remain remapped
so standard Save, New and Open shortcuts are unambiguous.

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
no icon theme is installed. **View or Tools > Customize Toolbar** provides the
complete 36-command legacy catalog: add/remove/reorder buttons, insert separators,
repeat commands, or reset the native default layout. Changes are staged until OK;
Cancel preserves the live toolbar and editor state. Repeated commands mirror the
same menu action without duplicating keyboard shortcuts.

Dock at the top, bottom, left or right, lock/unlock movement, choose 16-48 logical
pixel icons and icons-only, text-beside, text-below or text-only presentation.
Floating remains disabled. Dragged placement, visibility and customization persist
through settings and JPR. A layout must contain at least one slot; use View > Toolbar
to hide it. The existing 29-command default is retained, while optional commands
include Delete, previous search, formatting, color-list creation, user conversions
and Options. `ToolbarButtons` retains the legacy 100-byte array and inactive tail,
`ToolbarButtonCount` validates 0-100 (zero restores defaults), and native
`Jwpqt_ToolbarArea/IconSize/TextStyle/Locked` settings store presentation. Invalid
active IDs reject before changing the toolbar; unknown unrelated settings survive.

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
controls. These choices, plus personal/place-name inclusion and classical
dictionaries, persist through application settings and projects. **Options >
Dictionary** stages the same controls; accepted changes update an open lookup
without searching, flushing pending kana or changing results and selection.
Manual lookup changes also reach saved preferences. Unknown exclusion bits are
retained and disclosed, including through Default Settings.
Changing a checkbox does not discard results or start a search.

**Options > Dictionary > Exclude tagged senses** implements all 21 legacy
category exclusions, independently selectable:

| Tag | Category | Tag | Category | Tag | Category |
| --- | --- | --- | --- | --- | --- |
| `vulg` | Vulgar expressions | `X` | Rude/X-rated terms | `col` | Colloquialisms |
| `m-sl` | Manga slang | `sl` | Slang | `MA` | Martial arts |
| `id` | Idioms | `arch` | Archaisms | `obs` | Obsolete terms |
| `obsc` | Obscure terms | `ok` | Outdated kana | `abbr` | Abbreviations |
| `fam` | Familiar language | `pol` | Polite language | `hum` | Humble language |
| `hon` | Honorific language | `fem` | Female language | `male` | Male language |
| `pref` | Prefixes | `suf` | Suffixes | `oK` | Outdated kanji |

Filters use the dictionary's exact, case-sensitive tags, not text classification.
They remove matching senses rather than unrelated meanings; mixed recognized
groups retain allowed types. As in the source, an unrecognized tag stops filtering
the rest of that group. Filtering applies before accepted-result limits and
fallback decisions, including pattern, inflection, contingent and optional name
searches. Settings and JPR preserve the original mask bits 4 through 24; only
unknown bits 25 through 31 remain unapplied. Defaults resets these supported
filters while retaining unknown bits. Cancel and accepted preferences leave the
current query, pending kana, history and results intact until another search.

**Contingent** enables the source-style retry after eligible exact Japanese
searches fail. It requires Begin and End With and retains the engine's pattern,
ASCII, name-filter and work-limit safeguards. **Shift+Search** requests a forced
retry for that search only, relaxing the source's heuristic restrictions without
changing the saved preference. Ordinary searches do not inherit that forcing.

**Options > Dictionary > Search a document selection** defaults on. Opening
lookup searches the selected first-paragraph span, leaving the document unchanged.
It is not search-as-you-type. A new lookup still prefills the word under the cursor
without searching it when there is no selection. An existing query, including
pending kana, stays intact when automatic search is off or there is no selection.
If showing the window changes the query, the original automatic request is skipped.

Dictionary **History** opens a selectable query list; `Up` recalls older queries,
and `Down` moves toward newer entries and a blank draft, or opens the list when
the query has been edited. Recall resolves pending kana once, never starts a
search, and leaves the current results and document untouched. The chooser
supports Copy, Delete, OK and Cancel. Deletion takes effect immediately, including
when the chooser is subsequently cancelled; Cancel otherwise preserves the
query and its selection.

Completed searches, including zero-match queries, are remembered; failed searches
do not replace history or prior results. Exact duplicates move to the front.
The default source-sized budget permits at most 31 entries and 267 total Unicode
scalars. Oversized queries are not truncated, and an edited draft that cannot
be retained is not discarded by older-query navigation. History survives closing
the lookup, replacing its resources and restarting the application.

**Options > History** configures a per-history limit of 0..30000 storage cells
and saving on exit. Each history has its own budget. The application loads
`query-history.bin` from its configuration directory. All three
dictionary/search/replace lists round-trip and are available in their respective
lookup, Find and Replace windows. Settings,
history import and reload leave current queries, pending kana, results and
selections intact. Turning off automatic saving leaves any existing file unchanged.

**Tools > Query History** provides Save, Save As, Reload, Import and Clear.
Import replaces memory, not its source file; explicit Clear affects all three
lists and the configured archive, even when automatic saving is off. Legacy
`JWPxp.his` import requires confirming its original history capacity and code
page, which are not stored in that file. Its recent/workspace path tail is not
imported, and the legacy file is never changed or imported automatically.

History writes use atomic replacement, cooperative locking and an exact loaded
source check. Changed, deleted, corrupt or unreadable archives are not silently
overwritten. Save As can recover to a new file, and a failed exit save offers
Cancel to retain the workspace. A load or settings change that reduces history
pauses automatic saving when it might discard saved entries: increase the size
and Reload to recover them, or explicitly Save the smaller histories. Resource
reports disclose these warnings without writing files. Locks do not provide
race-free protection against external tools that ignore them.

### Document Find And Replace

**Edit > Find/Replace** opens modeless windows with local Kanji/ASCII/JASCII input,
shared Insert/Overwrite mode, and separate search/replacement histories. Up/Down
recalls text without searching; History offers Copy/Delete/OK/Cancel. Oversized
drafts are not discarded, and history deletion is immediate even on Cancel.
Opening a new window uses the selected first-paragraph text; reopening an existing
window preserves its draft. Ordinary Find does not erase the last replacement.

Search supports forward/backward matching, ASCII case folding, full-width
letter/digit equivalence, optional wrap and circular traversal through all open
documents. Find Next/Previous reuse the accepted search. All Files supersedes
the wrap checkbox. Case, width, wrap, all-files and keep-open policies persist in
configuration and JPR; direction starts forward in a newly opened window.

Replace Next confirms one match. Review Matches offers Yes, Skip, Yes to All and
Cancel; Replace All explicitly processes the whole selected document/workspace
scope without per-match prompts. Replacement works on original non-overlapping
snapshot matches, not newly inserted text. Normal next/review traversal excludes
the current starting match, as in the source; Replace All includes it. Searches
stay within paragraphs and preserve literal Unicode, including supplementary
characters. An empty replacement deletes matches.

All prospective native replacements are validated before editing. Work and match
counts are bounded across the workspace. Changes to documents or selections during
confirmation cancel unprocessed replacements rather than editing stale positions.
Read-only documents and active conversions are protected. Each accepted edit is
undoable in its document's native or Unicode history, preserving encoding and
metadata. Cancel leaves already accepted edits undoable and remaining text intact;
this is not an atomic cross-document transaction.

### Find In Results

With a dictionary, accumulated-results, kanji lookup/count or user-dictionary list
focused, **Ctrl+F** opens a modeless Japanese-input Find window. Its context menu
also offers Find, Next and Previous. Searches advance past the current logical
entry, support backward traversal and optional wrap, and select the whole matching
entry. They never match across separate entries or select dictionary grouping
labels as records. A list containing only the current match reports no other match.

Search history and accepted case/width/wrap/keep-open preferences are shared with
the workspace; replacement and all-files controls are omitted. F3/Shift+F3 repeat
in ordinary lists. Kanji result strips retain their existing F3 navigation and
offer repeated Find through the context menu. Finding does not search dictionaries,
edit documents or alter canonical insertion ownership. Sorting and refreshed
results supply fresh entry ranges. Invalid or oversized searches preserve selection;
newer selections or owner destruction during callbacks are not overwritten.

Result-list context menus also expose the recovered destination commands. **Insert
to Current File** preserves an existing destination selection and inserts at its
active endpoint; **Replace in Current File** is the explicit destructive variant.
**Insert to New File** creates a Japanese document, **Insert to Any File** chooses
an open document, and **Insert to Last File** repeats the last successful New/Any
destination while that document remains open. New/Any/Last restore focus to the
source results. Canonical rows, multi-row formatting and raw byte/JIS identity use
the same insertion path as the visible Insert button, and each destination change
is one undoable edit. Cancelling Any, closing Last, read-only documents, conversion
previews and reentrant tab changes do not redirect or partially apply insertion.

### Dictionary Result Ordering

Dictionary **Sort** cycles Reading, Length, Entry and Definition order. Hold
`Shift` to cycle backwards or `Ctrl` to reverse the current order without cycling.
Length uses headwords when the completed query contained kanji, otherwise readings;
editing the query afterwards does not reinterpret existing results. Each successful
sort removes exact duplicate entries, retains the first entry's provenance and
selects the first result. Copy and Insert continue to use the correctly mapped
display text and canonical entry. Sorting does not search, flush pending query
input or change query history; a new successful search resets the sort order.
Invalid or over-budget sorting preserves the previous results and selection.
The source comparison rules are applied deterministically with explicit work
limits, retaining distinct Unicode and structured metadata. `Ctrl` keeps toggling
rather than reproducing the legacy lock into reverse after twenty presses.

**Options > Dictionary > Compact results** places each headword and its
definitions in one paragraph, with comma-separated meanings. It persists as
`Dict_Compact` and applies to the next successful search. Existing results retain
their completed layout through sorting or failed searches even if the preference
changes. Copy preserves exact selected Unicode text and rich text; Insert keeps
the full canonical entries, including metadata, independently of display layout.
Priority grouping and search separators are native. Priority entries are identified
from the original terminal `/(P)/`, before category filtering, and are grouped
within their actual search/pass boundaries. Priority and Advanced separators are
independently configurable; unmarked adaptive searches retain the legacy shared
boundary behavior. Labels can be copied but never inserted as dictionary entries.
The view preserves original record provenance while inserting in visible order.
Explicit Sort removes presentation labels and starts from the displayed ordering;
later preference changes apply only to a new successful search. The three policies
persist as `Dict_PriorityEntriesFirst`, `Dict_PrioritySeparator`, and
`Dict_Adv_SeparatorMark` through Options, configuration and projects.
Plain left double-click inserts the actual clicked entry, not an older selection.
Section labels are never inserted; modified-click selection keeps its normal Qt
behavior, and reentrant selection/document changes cancel insertion safely.

The lookup's **Options** button opens the Dictionary settings tab; **User
Dictionary** reuses the application-owned editor and follows its availability.
Both commands are blocked during an active search and leave the query and results
alone. Options supports cancellation, and the user-dictionary window remains
independent when lookup closes.

With results focused, `Enter` or keypad `Enter` inserts the selected canonical
entries without searching again. With no selection it returns focus to the query.
Printable typing moves to the local query and uses its Kanji/ASCII/JASCII mode,
selection, overwrite and undo behavior; Copy and result navigation retain their
normal shortcuts. Typing alone does not submit a search.

**Options > Dictionary > Link Advanced with exclusion of personal and place
names** persists as `Dict_Link_Adv_NoNames` (default off). When enabled, checking
Advanced excludes both name categories; including either category turns Advanced
off. Each user toggle publishes one complete policy update without searching or
discarding pending kana/results/history. Imports preserve their specified values
until a user toggles a control rather than silently normalizing combinations.

**No Names** controls both name exclusions, retaining a mixed state for unequal
personal/place choices. **Names** temporarily includes both categories and
disables Advanced/Contingent retries without changing saved preferences. Name
filters apply to ordinary dictionary records as well as optional name passes.
**From Clipboard** explicitly searches its first paragraph; **Monitor Clipboard**
is default-off and persists as `MonitorClipboard`/`dict_watchclip`. It consumes
future external changes only while lookup is visible and available, never old
clipboard content merely on opening/enabling. Searches coalesce; editing,
pending kana, selection, hiding and disabling invalidate queued work. Invalid
or oversized input preserves the query/results, and application-owned copies
do not feed back into monitoring. Successful clipboard queries may enter saved
history; enable the option only when that is wanted.

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

Radical Lookup links the recovered variant groups, seeds the current JIS kanji
and extracts radicals from the first clipboard character. A quick stroke count
supports Exact, +/- 1 and +/- 2 alongside the explicit range controls. Its
arrows cycle Any, the selected-radical stroke estimate, through 30 and back to
Any; the estimate is a bounded stepping hint, not an automatic filter. Invalid
extraction preserves prior state. Clear cancels pending work, and changed
criteria clear stale results when automatic search is disabled.

Options > Kanji Lookup persists shared Auto Search and Rare Kanji Last through
settings and projects without searching or disturbing the current results.
Bushu matching sources, flexible kun readings, partial meanings, SKIP miscodes,
default reading/index types and radical variant/rare-choice presentation also
persist. User changes synchronize the two Bushu pages. Preferences unavailable
in the loaded metadata are retained with a visible fallback or disabled control;
they are not silently replaced by that fallback. Applying settings preserves
pending query input and results. Rare-choice dimming follows the original fixed
radical list, not a frequency estimate, and does not disable selection.
Bushu has the same bounded stroke stepping. In radical/code/reading result
strips, Enter inserts, I/F23 opens information, F2/F3 or period/comma moves
(Ctrl moves five), C copies the selection, Shift+C copies all without changing
selection, and F4 closes. Shift-click/Shift-Space on Copy also copies all.
Query fields keep their local Japanese input controls; Reading/Index remain
explicit searches. The final non-menu workflow audit remains in progress.

### Character information

Right-click a character and choose **Character Information**, or use `Ctrl+I`
at the caret/selection. Shift-right-click opens information directly. Each
request opens an independent modeless window, including requests from the
readings or **More Info** panes; the original windows remain open.

The document context menu also exposes Dictionary, Radical, Bushu,
Stroke/Bushu, SKIP, Spahn-Hadamitzky, Four-Corner, Reading and Index lookups,
the three input modes, Convert Selection and JIS Table. These are the same
actions as the main menus, so their enabled and checked state stays synchronized.
Character Information remains tied to the exact right-click target without
moving the document selection.

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
