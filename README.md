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

Run the editor, optionally opening a document:

```sh
/srv/tmp/jwpqt-build/src/qt/jwpqt [file]
```

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
candidate-bar position, default JWP code page, dictionary search policies and
exit/recent-file/query-history persistence and history capacity.
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
kanji lookup tools and project workspaces are available. Native document printing
and preview are described below. Remaining configuration, input/lookup controls,
legacy print tuning and other remaining command-parity gaps remain in progress.

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

JWP odd/even headers and footers support first-page suppression and the source
`&A`, `&C`, `&D`, `&F`, `&K`, `&L`, `&N`, `&S`, `&T`, `&P` and `&&` substitutions.
Date/time are captured once for the job and currently use `yyyy/MM/dd` and `HH:mm`.
Vertical JWP output preserves the source paper convention: Japanese glyphs are
counter-rotated while Latin and the recovered exception punctuation stay on the
horizontal baseline, for reading the paper after a clockwise quarter turn. Native
font shaping is retained; this is not a new top-to-bottom writing mode or a promise
of pixel-identical legacy bitmap/vertical-GSUB placement.

**Options > Printing** stores `Print.Font`, `Print.Size` and `Print.Auto` through
configuration and projects. The size is physical points in the dialog and tenths
of a point on disk, independently of screen font sizes. Automatic uses the document
font family; explicit families use Qt's native font fallback. Output is monochrome.
Custom legacy date/AM-PM formats, header-position tuning and ASCII grid-justification
settings remain retained and disclosed as unapplied.

PDF output is completed in a temporary file beside its destination and published
atomically. Invalid geometry/ranges, cancellation (including the final checkpoint)
or a print destination opened as a document during progress cannot destroy a prior
file. Settings, histories and the current project are also protected destinations.
Paper size, page/format/run counts, copies and rendering work are bounded; oversized
jobs report an error rather than silently truncating. Native spool errors are
reported, but cancellation cannot recall pages already sent to a physical printer.
PDF text/image/preview verification is the accepted printing gate for this port;
physical-printer testing is not required. This does not claim testing of every
printer backend or remove the retained legacy-formatting limitations above.

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
this is not an atomic cross-document transaction. Find in auxiliary result lists
is a separate remaining porting task.

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

This is not complete dialog parity: remaining history consumers and dictionary
options, radical automatic stroke estimation and stroke-tolerance shortcuts
remain open. Existing numeric ranges and portable dictionary search algorithms
do not substitute for those controls.

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
