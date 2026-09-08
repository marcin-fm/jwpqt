# Settings and Histories

Options stages changes; Cancel leaves effective settings unchanged. Default
Settings resets implemented values while preserving genuinely unknown fields.
Import Settings validates every supported assignment, including invalid earlier
values followed by valid overrides. Unsupported source lines remain preserved
and are disclosed rather than silently applied.

Settings are stored in jwpqt.cfg. Japanese System/Edit/List/KanjiBar/File/Big/Table
fonts are not desktop menu fonts. Missing or bitmap families use native
fallbacks. Print has a separate physical font role. Some remaining legacy
font roles and tuning controls are not implemented.

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
