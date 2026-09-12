# Japanese List Controls

JWPqt's Japanese-aware lists present dictionary results, character and kanji lookup data, counts, history, and editable dictionary rows with Unicode-capable Qt views and recovered selection behavior. Results remain structured independently of their displayed layout, so copying and inserting can use the intended headword, reading, or full record rather than accidental presentation labels.

In focused dictionary results, a plain click selects the current complete result. Ctrl+left-click or Ctrl+Space toggles it; Shift+left-click and Shift+Up/Down extend a row range. Up, Down, Home, End, Page Up, and Page Down move the current row; Ctrl navigation preserves existing selections. Space adds the current row, Ctrl+A selects all rows, and Ctrl+C or Ctrl+Insert copies the current canonical entry when no row set exists. Ctrl+E copies the headword and Ctrl+R copies the first reading, falling back to the headword.

Ctrl+I opens independent Character Information for the current entry's first suitable character. Ctrl+L or F5 sends a JIS-mapped character to Radical Lookup. Ctrl+F, Ctrl+S, or F8 opens within-result Find; Ctrl+N, F3, or F9 repeats it. F23 opens the local context menu. Copy uses selected display text where character-level selection is appropriate; Insert uses complete dictionary entries touched by selection.

The legacy list concepts survive without the Windows CE pointer conventions. Insert behavior is configured in Options, including separate paragraphs for complete rows. See [Adding or Editing Entries](help:IDH_DICT_USEREDIT) and [Searching and Results](help:IDH_DICT_RESULTS).
