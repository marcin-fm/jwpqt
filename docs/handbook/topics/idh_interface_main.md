# Main Display

The JWPqt main window contains a menu bar, configurable toolbar, document tabs, active editor, and status bar. Each tab owns its document text, selection, scrolling, format, and undo history. Opening an already-open path follows the configured duplicate-file policy: it can use the open document, reload it, or open an independent copy; noninteractive and replacement-oriented paths use the existing tab. The active tab is where typing, Save, and most editor commands operate.

The toolbar is visible by default and shares the menu actions for files, clipboard/history, search, input modes, conversion, dictionaries, kanji tools, and page layout. View > Toolbar hides or restores it. Narrow windows expose remaining actions through the toolbar extension button. View or Tools > Customize Toolbar can stage add/remove/reorder operations, separators, docking, lock state, icon size, and text presentation; menu actions and shortcuts remain authoritative.

The status bar exposes input and editing state. Its mode control selects Kanji, ASCII, or JASCII; the editing control switches Insert and Overwrite. In Kanji mode, romaji is composed into kana. F4 toggles Kanji and ASCII; Ctrl+K selects Kanji, Ctrl+Alt+A selects ASCII, and Ctrl+J selects JASCII. Insert toggles typing mode. These controls affect the active workspace behavior without changing the file's storage format.

Document title and modified state are native tab/window presentation rather than the exact legacy title-bar separator convention. Use File commands for named document actions and Window commands to navigate tabs. See [Changing the Current File](help:IDH_FILE_CHANGE) and [Input Modes](help:IDH_TEXT_INPUTMODES).
