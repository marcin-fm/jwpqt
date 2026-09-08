# Documents and Projects

Each tab owns its document, cursor, selection, encoding and undo history.
New and menu Open preserve other open documents. Opening an existing path
activates its tab. Save All visits unnamed files too; cancellation stops without
discarding unprocessed documents. Close All and Exit ask about every modified
document before discarding the workspace.

## Formats

Native JWP preserves paragraph formatting, page breaks, metadata and headers.
Text formats include UTF-8, UTF-7, UTF-16LE/BE, JFC, EUC-JP, Shift-JIS and
New/Old/NEC JIS. JFC reads UTF-8 first and legacy EUC when necessary, and writes
UTF-8. Byte-order marks and explicit encodings are retained where applicable.
Unmarked ambiguous files require an encoding choice rather than a silent guess.

Save As can transfer between formats after validation and any required loss
confirmation. Export Copy leaves the live file, history and saved baseline
unchanged. Neither command may overwrite another open document. Revert reloads
the current source. Delete clears the editor only after deletion succeeds.

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
