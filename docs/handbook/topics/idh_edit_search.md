# Search and Replace

Use **Edit > Find...** (`Ctrl+F` or `F8`) and **Edit > Replace...** (`Ctrl+H`, `Shift+F8`, or `Ctrl+R`). **Find Next** uses the standard shortcut or `F9`; **Find Previous** uses the standard shortcut, `F7`, or `Ctrl+B`. These modeless Qt dialogs have their own Kanji/ASCII/JASCII fields, share the window's insert/overwrite mode, and keep search and replacement histories separately. Reopening a dialog preserves its draft; Up, Down, and History recall text without searching.

Search is literal, not regular-expression matching, and stays within paragraphs. It supports forward or backward traversal, optional wrapping, ASCII case folding, and full-width equivalence for supported Latin letters and digits. Case and width options do not claim arbitrary Unicode or punctuation equivalence. Find Next and Previous reuse the accepted search. **All Files** walks open document tabs circularly and supersedes the Wrap Around control; direction begins forward in a newly opened dialog. The case, width, wrap, all-files, and keep-open choices persist in configuration and projects.

**Replace Next** confirms one match. **Review Matches** presents Yes, No, Yes to All, and Cancel. **Replace All** processes the original non-overlapping snapshot of matches in the selected document or workspace scope, never text newly inserted by replacement. An empty replacement deletes the matched text. Ordinary next/review traversal excludes its current starting match, whereas Replace All includes it.

JWPqt validates prospective native replacements before editing. Read-only documents, active conversions, invalid encodings, changed selections, or changed documents during confirmation stop unsafe remaining replacements. Work and match limits fail explicitly rather than silently truncating. Each accepted occurrence is independently undoable in its document; cancelling a multi-file review leaves completed edits undoable and unprocessed text intact.

See [Undo and Redo](help:IDH_EDIT_UNDO), [Input Modes](help:IDH_TEXT_INPUTMODES), and [Selecting Text](help:IDH_EDIT_SELECTING).
