# Clipboard Encoding

JWPqt can provide native clipboard information alongside external text representations. Clipboard options configure a chosen export format and an import format. Export defaults to **Shift-JIS**; import defaults to **Unicode**. Import may also use **Auto-detect**. The supported explicit formats are EUC-JP, Shift-JIS, New JIS, Old JIS, NEC JIS, Unicode, UTF-7, and UTF-8. JWPqt validates the selected format instead of guessing an unsupported encoding.

Unicode means UTF-16LE for this clipboard setting. The JIS choices retain their corresponding escape conventions, and invalid, truncated, or unsupported JIS escape sequences are rejected rather than decoded as arbitrary text. Encoding and decoding also enforce byte and character limits and require valid Unicode. A failed conversion must not silently truncate clipboard content.

For native selection Copy or Cut, JWPqt retains ordinary clipboard text plus the exact native JWP fragment, including original byte/JIS identity where relevant. This permits native paste to preserve the structured representation while external applications can use the configured text flavor. The application may additionally offer a clipboard bitmap and Unicode text; by default the bitmap is omitted and Unicode text is not omitted. Bitmap vertical and kanji-color choices are separate presentation options and do not alter the document's text or native tokens.

Clipboard encoding controls external representation, not a document's saved encoding or input mode. It does not turn Japanese Editing on, choose Kanji/ASCII/JASCII, or change the file format. Pasting is always an insertion operation, even in overwrite mode. Source text that cannot be safely decoded or placed in the target is rejected before damage occurs.

See [Using the Clipboard](help:IDH_EDIT_CLIPBOARD), [Edit Modes](help:IDH_TEXT_EDITMODES), and [Input Modes](help:IDH_TEXT_INPUTMODES). Use a document Save As or Export Copy choice, not clipboard encoding, when choosing the on-disk format.
