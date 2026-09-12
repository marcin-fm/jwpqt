# Edit Modes

JWPqt has two document editing engines. Japanese Editing is the native JWP engine. It retains JWP paragraph formatting, hard page breaks, native search and replacement behavior, document metadata, and portable document history. It is used for a new Japanese document and for text whose characters can be represented in JWP. Plain Unicode editing is for unrestricted Unicode text. It keeps the file's text format but does not carry the native JWP layout model.

Use **Edit > Input Mode > Japanese Editing** to change engines. Disabling it permits characters outside the JWP repertoire. Enabling it refuses unsupported characters rather than replacing them. The command does not change the saved file format. Before a change, JWPqt warns that Undo and Redo are cleared; changing to Unicode editing also removes JWP layout, structural hard page breaks, and document metadata. Finish any pending Japanese input before making a mode change.

This document mode is different from the typing input mode described in [Input Modes](help:IDH_TEXT_INPUTMODES). The latter chooses Kanji, ASCII, or JASCII input; it does not by itself alter the file format or document engine. Native Japanese Editing supplies conversion and native formatting, while the Unicode engine still accepts ordinary desktop text input and supports its Qt edit history.

Use the native engine when the document needs JWP-compatible formatting or conversion. Use unrestricted Unicode when the text needs characters outside JWP. Save As still offers the supported JWP and text formats. When text output would lose paragraph or page layout, headers, footers, metadata, or hard page breaks, JWPqt asks before writing it. Unsupported output characters fail before the destination is changed.

The status of the current document determines which native commands are meaningful. Do not treat this as the old Windows editor mode switch: it is a Qt desktop document-engine choice, with no Win32 compatibility layer. See [Formatting Paragraphs](help:IDH_EDIT_PARAGRAPH) and [Undo and Redo](help:IDH_EDIT_UNDO) for consequences of changing modes.
