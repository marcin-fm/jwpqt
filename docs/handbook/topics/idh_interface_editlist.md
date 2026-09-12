# Edit-List Controls

Editable lists are used for WNN user conversions and EDICT user-dictionary entries. They are modeless working-copy editors: Add, Edit, Delete, Move, Sort, and Import change only the dialog's local copy until Save validates and atomically publishes the complete dictionary. Cancel is the intentional discard action. Closing the window separately asks whether to save, with Save as the safe default; choosing No closes that working copy without publishing it.

Recovered keyboard commands are retained where applicable. Insert adds an item, Space edits it, Delete removes it, Ctrl+Up and Ctrl+Down reorder it, and Ctrl+C or Ctrl+Insert copies the current displayed entry without saving. Ctrl+I and a row context menu open Character Information for the row's first original JIS character. Ctrl+L or F5 seeds Radical Lookup from that character. Ctrl+F4 closes through the normal save-prompt path, and F23 opens the full row menu from the keyboard.

Import starts in the directory of the configured user file and parses every selected file before appending anything. User Conversion warns before additive import and defaults to No. Imported source-compatible records can be broader than newly created records; the entry editor retains invalid fields for correction rather than silently normalizing them. Sort uses source-style ordering with bounded work.

Insert to File sends the selected row into the active compatible document as one undoable operation. It refuses active conversion, read-only targets, or invalid insertion without mutating text. See [User Kana to Kanji Conversions](help:IDH_TEXT_USERKANJI) and [Adding or Editing Entries](help:IDH_DICT_USEREDIT).
