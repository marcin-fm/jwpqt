# Undo and Redo

Use **Edit > Undo** and **Redo** with the standard Qt desktop shortcuts. Native Japanese documents retain a portable JWP history; unrestricted Unicode editors retain Qt history. Native edits such as a conversion acceptance, a visual-line Cut, paragraph join, selection replacement, formatting change, and structural page break are recorded through the native model so Undo restores text together with applicable formatting and metadata.

Japanese conversion is deliberately transactional. Selecting or cycling a candidate changes the preview; accepting it with `Enter` or `Escape` is one undoable conversion. Explicit selected-romaji conversion is also one operation: Undo restores the whole original selection. If Return or Ctrl+Return follows an active candidate, acceptance and paragraph or hard-page-break insertion are separate Undo steps. This permits either part to be undone without corrupting the reading.

Set native history depth in **Options > History**. It accepts 3 through 1000 levels and defaults to 50. Reducing the limit keeps the nearest undo and redo entries while permanently discarding older entries; it does not alter document text or saved/dirty state. Increasing the value does not recover discarded entries. Finish an active conversion before changing the limit. New documents, settings, and projects retain the selected native depth.

The native depth setting never truncates Unicode text or silently clears a Unicode editor's Qt stack. However, changing **Japanese Editing** is a major engine operation: after confirmation it clears Undo/Redo, and changing to Unicode editing removes JWP layout, hard page breaks, and metadata. This is announced before it occurs.

Each accepted Find/Replace occurrence is undoable in its owning document. Cancelling a multi-file review preserves already accepted edits as undoable changes; it is not a global rollback. See [Search and Replace](help:IDH_EDIT_SEARCH), [Formatting Paragraphs](help:IDH_EDIT_PARAGRAPH), and [Explicit Kanji Conversion](help:IDH_TEXT_EXPLICITKANJI).
