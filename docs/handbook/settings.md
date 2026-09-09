# Settings and Histories

Options stages changes; Cancel leaves effective settings unchanged. Default
Settings resets implemented values while preserving genuinely unknown fields.
Import Settings validates every supported assignment, including invalid earlier
values followed by valid overrides. Unsupported source lines remain preserved
and are disclosed rather than silently applied.

Settings are stored in jwpqt.cfg. Japanese System/Edit/List/KanjiBar/File/Big/Table
and Bitmap fonts are not desktop menu fonts. Missing families inherit the parent
role, including its size; the stored unavailable name and a diagnostic remain.
Large characters fit their pane on resize. Table remains 16 logical pixels.
Print has a separate physical font role.

Show all installed families starts disabled. Japanese and printer font selectors
normally list only families that advertise Japanese support; enabling it exposes
all public system families. The ASCII selector always remains complete. Changing
the checkbox does not change a chosen family, including a manually entered name
or `.f00` path, until Options is accepted.

Insert complete result-list entries on separate lines follows the original list
insertion policy and starts enabled. Dictionary, accumulated-result, count and
user-dictionary rows then become separate paragraphs, including the terminating
paragraph after the final row. Disable it to join complete rows compactly: eligible
ASCII boundaries use a tab, while Japanese or already-spaced boundaries join
directly. Character-level insertion from information and kanji lookup controls is
not changed. The whole insertion remains one Undo operation in native Japanese and
unrestricted Unicode documents.

Selection autoscroll starts enabled with the original 100 ms repeat delay.
While extending a mouse selection within one-third of a text line from the top
or bottom edge, the document moves one line immediately and continues at that
delay. Disable it to keep an edge drag within the visible page. A delay of zero
uses Qt's shortest event-loop interval. The setting applies to every existing
and new document without changing text, selection, scroll position or undo.

Bitmap controls the image included when copying document text; Automatic inherits
File. Omit bitmap removes only that representation, not text or rich formats.
Images have white paper and preserve the selected text without changing the
document or undo. Images are bounded at 262,144 selected UTF-16 positions,
8,192 pixels per edge and 16 million pixels; an omitted oversized image is
reported while the text is still copied. Vertical clipboard mode uses the
bitmap font's own settings, even with Automatic retained. Japanese glyphs are
counter-rotated for reading after a clockwise turn; Latin stays horizontal, as
do the source punctuation exceptions in raster fonts. It does not rotate the whole image.
Color clipboard mode applies persistent kanji colors only when list coloring
is active, never selection or conversion highlights. Both settings can be
cancelled or saved like the other font options. Raster punctuation and small
kana use the original ink-bound positioning rules. The raster header does not
select alternate vertical glyphs; non-square raster rotation is rejected safely.
TrueType fonts with a vertical substitution feature use their alternate glyphs
and rotate Japanese punctuation as well. The private face retains the original
text in PDF extraction and does not change horizontal fonts or saved settings.
Fonts without that feature keep the native fallback. Other font tuning remains
explicitly unfinished.

ASCII and legacy extensions selects a separate single-byte font at the Japanese
role's height. Even when that family contains Japanese, its private restricted
face cannot replace Japanese glyphs. The original family name is saved, and
missing families report a native fallback. Legacy ASCII Size/Auto values are
retained but do not change the matched height. Copy images and printing share
this selection; optional Latin ligatures are disabled for individual-character
rendering and PDF text. No system fonts are modified or redistributed, and their
original licensing and embedding restrictions remain. Native documents preserve
the byte/JIS distinction even for identical Unicode: bytes use the ASCII face and
stay upright, while JIS characters use the Japanese face and vertical rules.
Copy images and PDF bodies/headers retain this display identity without changing
text or undo. Switching to unrestricted Unicode removes native display metadata.

You can enter a legacy .f00 filename instead of a font family, including Print.
Relative names use the settings directory, not a guessed Windows directory.
The native renderer loads the original packed or holey JIS bitmaps as private
pixel-outline faces. Screen roles use their original height; Big fits its pane,
Table stays 16 px, and Print uses its physical point size. Unavailable or invalid
screen fonts produce a diagnostic and inherited fallback. Invalid explicit print
fonts fail before output is written. Files stay unchanged and are not installed
system-wide or included with the program. Their original licensing still applies.
Private font caching is bounded; closing the application releases it.

## Colors

Options > Colors stages information-heading, kanji-list and uncommon-kanji colors,
list mode and uncommon coloring. Enter #RRGGBB, use Choose, or select Inherit.
Absent source overrides leave the existing native INI color store authoritative;
explicit imported values take precedence. Inherit immediately returns to the
stored native policy. Settings and projects retain these choices; Cancel does
not change them. Existing and inactive native documents update without editing
text or undo. Kanji Color Options also updates the corresponding stored choices.

Information headings adapt when necessary for readable theme contrast. Special
Windows palette references are preserved rather than mistaken for RGB; unsupported
heading/list references are disclosed, and uncommon color uses the source green
fallback. Inherit can explicitly clear such a reference. Uncommon-character marks
are small dots in conversion and lookup bars, not characters added to copied text.
Rare-radical deemphasis starts enabled, matching the source; explicit saved values
are retained. The heading color also highlights entries from classical and user
dictionaries, and the Advanced search label. Priority and contingent labels stay
ordinary. Sorting keeps each retained entry's source highlight; accumulated results
keep it too. Changing the color or theme does not rerun a query, replace its results,
or change selected/copied/inserted text.

## Customize the Toolbar

View or Tools > Customize Toolbar opens the complete command catalog. Select a
command on the left and Add it after the selected toolbar slot. Select a slot on
the right to Remove, Move Up or Move Down. Separator is a reusable catalog item;
commands may also repeat. Reset Layout restores the native default order without
changing unrelated settings. At least one slot remains; View > Toolbar hides an
unused toolbar instead. Cancel discards all staged changes.

Choose top/bottom/left/right docking, lock movement, icon size (16-48 logical
pixels) and icon/text presentation. Floating is disabled. The menu actions remain
authoritative, including enabled and checked state; duplicate buttons do not add
duplicate shortcuts. Docking and customization save with normal preferences and
projects. Imported legacy toolbar IDs and inactive configuration bytes are
preserved. Toolbar changes do not edit document contents or clear Undo.

## Persistent Histories

Query history stores dictionary, search and replacement lists in query-history.bin.
The configured capacity applies independently to each list. At the default
300-cell capacity, each history allows 31 entries and 267 text scalars in total.
Recall never silently truncates a draft. Oversized entries may be searchable
without being remembered.

Tools > Query History provides Save, Save As, Reload, Import and Clear. Legacy
JWPxp.his import requires its original capacity and code page because the file
does not identify them. Import never rewrites the original file or imports its
unrelated path tail. Search and replacement histories survive even when using
only the dictionary interface.

Reducing capacity can remove in-memory entries. When a fuller archive exists,
automatic saving pauses until Reload with sufficient capacity or explicit Save.
Changing, creating, deleting or corrupting an archive externally blocks stale
overwrites. Save As permits recovery to a new file. Turning automatic history
saving off leaves an existing file untouched; explicit Clear still clears all
three lists and the configured native archive after confirmation.

Learned conversion choices controls how many kana-to-kanji preferences are kept
(10-2000, default 200). Reducing it retains the most recent stored choices that
fit; increasing it creates empty slots. A loaded conversion resource resizes
immediately after Options is accepted. Finish or cancel an active candidate
preview first so cancellation cannot restore an obsolete preference capacity.
The setting is preserved in normal settings and projects.

Exit asks about modified documents and failed persistence. Cancel keeps the
workspace; Discard can exit without overwriting the conflicting archive.

[Find and Replace](editing.md) | [Installation paths](installation.md) | [Contents](start.md)
