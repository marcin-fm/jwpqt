# Single-Window Multi-Document

JWPqt uses one main window with independent document tabs. This preserves the practical intent of JWPce's single-window multi-document model while making open documents visible and directly selectable. Each tab has independent text, format, selection, scroll position, saved-state tracking, and undo history. Dictionary and character-information tools are shared modeless windows and insert into the currently active compatible tab, even if the tab that opened them was later closed.

New and Open add documents without replacing existing tabs. Opening a path already open selects its tab and keeps unsaved changes intact. File > Close removes only the current tab after any required save decision. Close All and Quit collect save/discard/cancel decisions before discarding any document. If the last tab is closed, the application can ask whether to exit; a negative response leaves a clean Japanese document available.

Window > Next File and Previous File use Ctrl+Tab/Ctrl+Shift+Tab or Ctrl+PageDown/Ctrl+PageUp. Window > Files uses Alt+W to choose an open document. The window close policy is configurable: an option can make the title-bar close control close only the current file. Ctrl+close forces file close and Alt+close forces application exit; explicit Quit exits after save checks.

The Web port keeps document tabs but does not support JPR referenced-file projects. See [Changing the Current File](help:IDH_FILE_CHANGE), [Closing Files](help:IDH_FILE_CLOSE), and [Working with Projects](help:IDH_FILE_PROJECT).
