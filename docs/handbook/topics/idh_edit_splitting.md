# New Paragraphs and Splitting Paragraphs

Press Return to accept an active WNN candidate, then insert an ordinary paragraph. `Shift+Return` also inserts an ordinary paragraph, following JWP behavior rather than creating a Unicode soft-line separator. `Ctrl+Return` accepts an active candidate and inserts a structural hard page break through **Format > Insert Page Break**. Conversion acceptance and the following structural edit are separate Undo steps.

Native paragraphs carry formatting. When a paragraph is split, the JWP document model preserves its paragraph structure instead of treating the edit as a plain-text newline alone. Formatting actions apply to the caret paragraph or selected paragraphs, and page breaks remain structural. This is why visual wrapping does not create paragraphs and why line-width changes do not modify the document text.

At a native paragraph boundary, ordinary Backspace and Delete perform the recovered JWP paragraph join. Joining through a hard page break removes that break, with one source-specific exception: if an empty paragraph immediately precedes a page-break paragraph, the empty paragraph is removed and the page break remains. The surviving paragraph retains its format. One Undo restores text, formatting, and page-break structure together.

Selected Backspace and Delete use the same metadata-aware route. A reversed selection or a selection crossing one or more hard page breaks is removed without silently restoring plain text or losing paragraph data. By contrast, `Shift+Backspace` and `Shift+Delete` delete to the start or end of the current wrapped visual line when there is no selection; they do not place text on the clipboard.

Finish an active conversion before operations that invalidate its source. Use [Formatting Paragraphs](help:IDH_EDIT_PARAGRAPH) for layout values, [Using the Clipboard](help:IDH_EDIT_CLIPBOARD) for line and selected deletion, and [Undo and Redo](help:IDH_EDIT_UNDO) to restore structural edits.
