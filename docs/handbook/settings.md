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

Bitmap controls the image included when copying document text; Automatic inherits
File. Omit bitmap removes only that representation, not text or rich formats.
Images have white paper and preserve the selected text without changing the
document or undo. Images are bounded at 262,144 selected UTF-16 positions,
8,192 pixels per edge and 16 million pixels; an omitted oversized image is
reported while the text is still copied. Vertical/color-list bitmap policies
and some other font tuning remain unimplemented.

ASCII and legacy extensions selects a separate single-byte font at the Japanese
role's height. Even when that family contains Japanese, its private restricted
face cannot replace Japanese glyphs. The original family name is saved, and
missing families report a native fallback. Legacy ASCII Size/Auto values are
retained but do not change the matched height. Copy images and printing share
this selection; optional Latin ligatures are disabled for individual-character
rendering and PDF text. No system fonts are modified or redistributed, and their
original licensing and embedding restrictions remain. Unicode shared by both
legacy bytes and JIS codes currently uses the Japanese face; exact distinction
between those original representations remains a separate fidelity gap.

You can enter a legacy .f00 filename instead of a font family, including Print.
Relative names use the settings directory, not a guessed Windows directory.
The native renderer loads the original packed or holey JIS bitmaps as private
pixel-outline faces. Screen roles use their original height; Big fits its pane,
Table stays 16 px, and Print uses its physical point size. Unavailable or invalid
screen fonts produce a diagnostic and inherited fallback. Invalid explicit print
fonts fail before output is written. Files stay unchanged and are not installed
system-wide or included with the program. Their original licensing still applies.
Private font caching is bounded; closing the application releases it.

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

Exit asks about modified documents and failed persistence. Cancel keeps the
workspace; Discard can exit without overwriting the conflicting archive.

[Find and Replace](editing.md) | [Installation paths](installation.md) | [Contents](start.md)
