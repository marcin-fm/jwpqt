# Selecting Text

Use Shift with movement commands to extend the existing selection. Home and End select to the current wrapped visual-line edge; Ctrl+Home and Ctrl+End select to document edges. `Ctrl+Left` and `Ctrl+Right` use recovered source word classes, skip source whitespace, and retain the katakana long-vowel rules. `Ctrl+Left` stops at a visual-line start before crossing it. `Ctrl+[` and `Ctrl+]` select through matching nested braces when Shift is held.

`Ctrl+W` selects the source-style word at the caret. The selection rules distinguish ASCII, punctuation, JASCII, kana, and kanji; whitespace advances to the following word, while a caret at paragraph end selects the preceding word. Kana long-vowel and repetition-mark rules are retained. `Ctrl+Shift+W` selects the entire current wrapped visual line. Both commands first finish pending Japanese input and do not edit text. `Ctrl+A` selects all document text.

Double-click or Ctrl+left-click selects a source-style word with the same boundaries. Shift+left-click is deliberately different: it opens Character Information for the exact pointed character. Alt+left-click, a held unmodified click, the Menu key, and Shift+F10 open the editor popup rather than extending text selection. Pending kana is committed before a mouse press relocates the caret.

Selections are safe across native structure. Cut, Backspace, and Delete operate through the structured model when the range crosses hard page breaks; formatting and page-break metadata are not silently lost. Clipboard copy of a native selection retains its ordinary text and exact native fragment. Typed input replaces only the selection, including in overwrite mode; it deliberately does not overwrite following text as well.

Select printable ASCII/tabs in one paragraph for explicit romaji replay with `F2` or `Ctrl+>`. Select kana and convert it again for WNN candidates. See [Moving Around the Document](help:IDH_EDIT_MOVING), [Using the Clipboard](help:IDH_EDIT_CLIPBOARD), and [Explicit Kanji Conversion](help:IDH_TEXT_EXPLICITKANJI).
