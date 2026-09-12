# Working with Microsoft's Global IME

This legacy topic has a different current answer. JWPqt is a Qt desktop application and does not provide or emulate Microsoft's Global IME or the Win32 IMM mode switch. Its own Kanji input mode composes ordinary desktop romaji into kana and supplies WNN candidates. An operating-system input method, including an IME supplied by the desktop, remains responsible for its own normal Unicode input into applicable Qt fields.

Use **Edit > Input Mode > Kanji** (`Ctrl+K`) for JWPqt's portable romaji-to-kana composer. Use **ASCII** (`Ctrl+Alt+A`) for literal characters or **JASCII** (`Ctrl+J`) for recovered full-width mappings. These are JWPqt controls, not a request to change the system input method. Switching modes commits pending kana and accepts a visible candidate. Query fields have their own K/A/J controls so a dictionary or Find/Replace query can be entered without changing the document's mode.

The historical `Ctrl+M` command controlled a Windows IMM conversion mode. On Linux native documents, `Ctrl+M` instead discards pending native romaji and displays a one-time notice that input-method conversion belongs to the desktop. Ordinary Unicode editors retain normal Qt toolkit handling. Do not expect the command to choose or configure a Microsoft IME, and do not depend on it for document conversion.

Desktop IME commits honor the current insert/overwrite setting where their explicit replacement range permits; explicit IME replacement ranges remain authoritative. JWPqt's composer, JASCII input, and ordinary IME commits are scalar-safe and undoable. Invalid input, field limits, validators, and read-only targets must not erase text before rejection. The same behavior applies in native Japanese editing forms and query fields.

For JWPqt conversion procedures, see [Input Modes](help:IDH_TEXT_INPUTMODES), [Entering Hiragana](help:IDH_TEXT_HIRAGANA), and [Inline Kanji Conversion](help:IDH_TEXT_INLINEKANJI). This replacement retains the legacy subject but documents the supported Qt desktop/Web boundary rather than obsolete Win32 instructions.
