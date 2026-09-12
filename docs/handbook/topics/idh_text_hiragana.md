# Entering Hiragana

Select **Edit > Input Mode > Kanji** or press `Ctrl+K` to enter hiragana through JWPqt's romaji composer. Type printable desktop romaji in the editor. The composer keeps an unfinished sequence at the caret until it can produce kana or until an action requires it to finish. Normal lowercase syllables produce hiragana. The completed kana remains editable text; it is not an operating-system composition window.

The composer preserves the recovered Japanese input rules, including ambiguous endings and extendable syllables. Continue typing when more letters can change the result. An unmodified `Caps Lock` in Kanji mode flushes ambiguous pending kana before the desktop changes its lock state. Moving the caret with ordinary navigation, Tab, Page keys, Home or End accepts an active WNN candidate before moving. A mouse press likewise commits pending kana at its original caret before the mouse moves it.

Hiragana can be the source for kana-to-kanji conversion. With WNN data loaded, JWPqt identifies automatic conversion spans, waits for a key that can extend the reading, and applies a terminal or longest-prefix candidate; unmatched text remains at the caret. The candidate strip below the editor shows the current reading and candidate. Click a candidate, or use `Space`, `Shift+Space`, or Convert to cycle it. `Enter` or `Escape` accepts the displayed result as one undoable conversion.

To convert existing roman text instead, select printable ASCII or tabs in one paragraph and invoke Convert with `F2` or `Ctrl+>`. Lowercase romaji becomes selected hiragana. The selection cannot exceed 65,535 input or output cells; incomplete or unsupported input leaves it unchanged. A second Convert can begin ordinary kana conversion. See [Explicit Kanji Conversion](help:IDH_TEXT_EXPLICITKANJI) and [Inline Kanji Conversion](help:IDH_TEXT_INLINEKANJI).

The exact kana text can also be inserted through the JIS Table. For direct unrestricted Unicode input, disable Japanese Editing as explained in [Edit Modes](help:IDH_TEXT_EDITMODES).
