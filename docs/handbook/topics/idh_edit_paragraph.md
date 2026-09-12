# Formatting Paragraphs

Choose **Format > Paragraph...** (`Alt+Shift+F`) to edit the native formatting of the paragraph at the caret or all selected paragraphs. JWPqt applies one requested format over that defined scope and records the change in portable native Undo. The values drive the Qt rich document layout: paragraph indents, hanging indents, proportional spacing, and structural hard page breaks are respected by editing, preview, and printing.

Use the dialog for a paragraph's layout, not for temporary wrapped-line appearance. Wrapped visual lines are derived from available width and the selected line-width policy. A paragraph remains one logical paragraph until a structural edit splits it. This distinction matters when Copy/Cut operates on the current visual line, while native formatting follows stored paragraph boundaries.

The dialog validates combinations that cannot be laid out safely. Invalid hanging indents or invalid page-width combinations report an error but keep the dialog open and retain the entered values for correction. JWPqt does not silently rewrite an invalid measurement or drop unrelated formatting. Existing JWP files retain saved page layout; page defaults apply to new Japanese documents and text imports through **Options > Default Page**.

For document-wide formatting, use **Format > Format File...** (`Ctrl+Alt+F`). Configure paper geometry through **Format > Page Layout...** (`Alt+L`). Use **Format > Insert Page Break** or `Ctrl+Return` for an explicit structural break. A normal Return and Shift+Return both insert ordinary paragraphs, not a Unicode soft-line separator. At a native boundary, Backspace or Delete joins paragraphs using the structured model and preserves the surviving paragraph's format.

Native formatting is removed only after confirmation when Japanese Editing is disabled, and text output warns when it would lose such layout. See [Formatting Text](help:IDH_EDIT_FORMAT), [New Paragraphs and Splitting Paragraphs](help:IDH_EDIT_SPLITTING), and [Undo and Redo](help:IDH_EDIT_UNDO).
