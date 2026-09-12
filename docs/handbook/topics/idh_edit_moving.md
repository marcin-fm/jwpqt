# Moving Around the Document

Use arrow keys, Page Up/Down, Home, End, and Tab for ordinary movement. Home and End move to the edges of the current wrapped visual line. Add Ctrl to move to document edges; add Shift to extend the existing selection. Left/Right, Up/Down, Home/End, Page keys, and Tab first accept an active WNN candidate so a conversion preview cannot be left behind at the wrong location.

`Ctrl+Left` and `Ctrl+Right` use JWPqt's recovered source word classes rather than Qt locale word boundaries. They skip source whitespace and retain katakana long-vowel behavior. `Ctrl+Left` stops at the beginning of the current wrapped visual line before crossing it; invoke it again there to move into the preceding line or paragraph. Add Shift to either command to extend selection. `Ctrl+W` selects the source-class word at the caret, while `Ctrl+Shift+W` selects the current wrapped visual line.

Curly-brace navigation is available with `Ctrl+[` and `Ctrl+]`. When the caret is on or immediately after a brace, the command follows nested matching braces. Otherwise it moves to the nearest brace on the current wrapped visual line, preferring a preceding brace. Shift extends the selection; an unmatched brace leaves the cursor unchanged.

Mouse behavior follows the same source word boundaries. Double-click or Ctrl+left-click selects a word. Shift+left-click opens Character Information for the exact character, while Alt+left-click opens the editor popup. Holding the unmodified left button without dragging opens the same popup after the platform double-click delay. The Menu key and Shift+F10 open it at the caret.

Use [Selecting Text](help:IDH_EDIT_SELECTING) for selection commands and [New Paragraphs and Splitting Paragraphs](help:IDH_EDIT_SPLITTING) for movement across document structure.
