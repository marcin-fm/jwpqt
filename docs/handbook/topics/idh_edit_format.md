# Formatting Text

Formatting in a native Japanese document is paragraph-based, not a collection of hidden inline desktop styles. **Format > Format File...** (`Ctrl+Alt+F`) applies the requested native paragraph formatting across the document. **Format > Paragraph...** (`Alt+Shift+F`) applies it at the caret paragraph or across selected paragraphs. The rich Qt layout then uses the saved indents, proportional spacing, and hard page breaks when displaying or printing the document.

Use paragraph formatting for the document's native layout characteristics. It is retained by the JWP engine and participates in portable Undo. If an invalid hanging indent or a page-width combination is entered, JWPqt reports the problem without closing the editor, allowing the retained values to be corrected. This is preferable to silently clamping formatting or changing unrelated paragraphs.

The document's page geometry is configured separately through **Format > Page Layout...** (`Alt+L`). New Japanese documents and text imports can inherit defaults from **Options > Default Page**; existing JWP files retain their saved layout. **Format > Insert Page Break** (`Ctrl+Return`) inserts a structural hard page break. It is not a Unicode soft-line separator and is separate from a normal paragraph created with Return or Shift+Return.

Formatting is preserved only while a document remains in Japanese Editing and a format that supports it. Turning Japanese Editing off warns that JWP layout, hard page breaks, and metadata will be removed. Saving as text likewise asks before losing layout-related information. The saved format is not changed by selecting an input mode, by normal wrapping, or by using the formatting dialogs until a document write is requested.

See [Formatting Paragraphs](help:IDH_EDIT_PARAGRAPH), [New Paragraphs and Splitting Paragraphs](help:IDH_EDIT_SPLITTING), and [Edit Modes](help:IDH_TEXT_EDITMODES).
