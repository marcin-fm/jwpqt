# Using the Clipboard

Use **Edit > Cut**, **Copy**, and **Paste** with the standard desktop shortcuts. Copy also has `Ctrl+Insert`; Paste accepts `Shift+Insert` and `Ctrl+Shift+Insert`. These paste the ordinary clipboard, and on X11 they do not substitute the primary selection. JWPqt finishes pending kana before the command so clipboard text and document state agree.

With a selection, Copy and Cut use it normally. A native JWP selection preserves ordinary clipboard text and the exact native JWP fragment, including ranges that span structural hard page breaks. Cut removes the selection through the structured document model as one undoable edit. It is disabled for a read-only document. Lookup and dictionary insertion similarly replace the active selection as one undoable insertion, but are protected against read-only targets, invalid Unicode, surrogate-pair splits, and active conversion previews.

With no selection, `Ctrl+C` copies the current wrapped visual line and restores the exact cursor. `Ctrl+X` cuts that visual line as one undoable edit. Empty lines do nothing. `Shift+Backspace` and `Shift+Delete` are non-clipboard line deletion commands: with a selection they delete it; otherwise they delete to the beginning or end of the current wrapped visual line. A line-edge no-op does not dirty the file.

Ordinary Backspace and Delete at a native paragraph boundary join paragraphs rather than performing a plain newline edit. This preserves the surviving format and correctly handles hard page breaks. Clipboard operations do not depend on the insert/overwrite toggle: pasting remains insertion even while typed ASCII, kana, JASCII, or ordinary IME commits honor overwrite mode.

Configure external text representation through the Clipboard options described in [Clipboard Encoding](help:IDH_EDIT_ENCODING). For selection boundaries see [Selecting Text](help:IDH_EDIT_SELECTING); for recovery see [Undo and Redo](help:IDH_EDIT_UNDO).
