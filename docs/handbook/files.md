# Documents and Projects

Each tab owns its document, cursor, selection, encoding and undo history.
New and menu Open preserve other open documents. When a path is already open,
Display And Files can ask whether to use the open document, reload it or open an
independent copy; it can also choose either non-destructive action automatically.
Using or reloading an open document retains its established file format. Scripts
and replacement-oriented APIs deterministically use the existing tab instead of
opening a modal prompt. Save All visits unnamed files too; cancellation stops
without discarding unprocessed documents. Close All and Exit ask about every
modified document before discarding the workspace.

You can drop one or more local files on the main window. They open in source
order through the same format detection, project and duplicate-file policies as
menu Open. An unreadable file reports its error without preventing later files
in the same drop from opening. Directories and nonlocal URLs are not accepted.

Closing the last tab normally asks whether to exit. No keeps a clean unnamed
Japanese document; it does not restore edits you already chose to discard.
Display And Files settings can disable this confirmation or make the window
close button close only the active file. Alt-close forces program exit and
Ctrl-close forces file close. Explicit Quit always uses the whole-workspace
save checks. The optional startup dictionary opens only without an explicit
document/project and with available dictionary resources.

Command-line startup accepts multiple documents and projects and processes them
in the order supplied. Projects append to documents already opened by an earlier
argument. An unreadable path is reported without preventing later paths from
opening, and a repeated path activates its existing tab. Noninteractive startup
returns a failure only when no supplied path can be opened.

**Restore main window** retains its normal position, size and maximized state.
Minimizing does not replace the saved normal bounds. Character Information,
Dictionary, Count, More Info and both user-dictionary windows independently
remember their last size and position. New windows use those values; changing
settings does not relocate windows already open. Independent information windows
remain independent, with the last moved/resized one supplying the next default.
Placement uses native logical pixels and is clamped to an available screen when
an old monitor is absent. Dimensions and restore flags travel with settings and
projects.

**Restore named files from the previous session** is enabled by default, matching
JWPxp, and can be turned off explicitly. It uses `last-session.jpr` in the
application configuration directory. Files reopen in their saved formats
and tab order before command-line files are opened or activated. Current preferences
remain authoritative; this archive does not become the current project. Missing or
invalid files are reported while usable entries open. Existing buffers are retained.
Exit records only currently named files after save/discard checks, never unsaved text
or unnamed buffers. Skipped references are omitted on the next successful save.
Disabling this option leaves the archive unchanged. A corrupt or externally changed
archive is preserved; failed saving offers Cancel or exit without updating it.
Resource-report does not restore tabs, and query-history saving is independent.

## Formats

Native JWP preserves paragraph formatting, page breaks, metadata and headers.
The source's Normal JWPce (`.jce`) and JWP (`.jwp`) choices use the same native
structured format. New Japanese documents default to `.jce`; existing `.jwp`
documents retain their extension.
Text formats include UTF-8, UTF-7, UTF-16LE/BE, JFC, EUC-JP, Shift-JIS and
New/Old/NEC JIS. JFC reads UTF-8 first and legacy EUC when necessary, and writes
UTF-8. Byte-order marks and explicit encodings are retained where applicable.
Unmarked ambiguous files require an encoding choice rather than a silent guess.

Interactive Open offers a best-effort recovery when a structured JWP document
has a valid header and metadata but a damaged paragraph stream. The warning
defaults to No and reports complete paragraphs, any retained partial paragraph
and ignored trailing bytes. Accepted content opens as modified; use Save As to
preserve the damaged source while writing a strict recovered document. Valid
packed JWPxp x86 undo payloads are skipped, as they were by the source loader,
and are not restored into native history. Invalid headers, metadata, malformed
undo payloads and safety-limit violations are fatal; only the recovered x86
layout is recognized. Automated opens, project restoration and Revert never
recover implicitly.

Save As can transfer between formats after validation and any required loss
confirmation. Export Copy leaves the live file, history and saved baseline
unchanged. Neither command may overwrite another open document. Revert reloads
the current source. Delete clears the editor only after deletion succeeds.

## Previous-Version Backups

Options can **Keep the previous disk version** when saving (off by default).
The backup appends `_BAK` to the complete filename: `notes.txt_BAK`. Save, Save As
and Export Copy preserve the destination's previous bytes, including its old
encoding, before publishing the new file. New targets leave any old backup alone.
An encoding error or backup failure does not replace the document; a successful
backup followed by failed document publication may update the backup to the
still-current disk version. Open/protected backup paths and backup symlinks are
rejected. Backups retain ordinary permissions, not every filesystem attribute.

## JPR Workspaces

Open Project can replace or append. Files and settings are validated before
publication; cancellation keeps open buffers. Append retains already-open dirty
documents. Legacy Windows paths require explicit equivalent Linux directory
mappings. The process working directory is not changed.

Save Project stores references, settings and native format/tab metadata, not
unsaved text or undo histories. Save the documents first, or explicitly choose
references only. Dirty unnamed documents cannot silently disappear from a
saved workspace. Unsupported imported settings remain visible and retained.

[Settings and history safety](settings.md) | [Contents](start.md)
