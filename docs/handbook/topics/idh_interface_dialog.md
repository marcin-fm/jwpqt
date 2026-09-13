# Dialog Boxes

JWPqt uses native Qt dialogs. Modal dialogs stage a decision and prevent the owning operation from continuing until they are accepted or cancelled. Options is staged: Cancel leaves effective settings unchanged, while accepted changes apply validated values. Page Layout follows the same principle. Help buttons in these dialogs open the embedded handbook without accepting or cancelling the dialog.

Several working tools are modeless, including dictionary search, accumulated dictionary results, Character Information, kanji lookup tools, and user-dictionary editors. They can remain visible while the main window is used. A modeless insertion target is the current active compatible document, not necessarily the document active when the tool opened. Insertion checks for a read-only target, invalid text, a selection splitting a surrogate pair, or an active conversion before changing anything.

Some dialog roles remember their geometry independently. Restored placement is bounded and clamped to available screens; importing settings does not relocate already-open windows. Dynamic Windows CE maximize/minimize behavior is not an interface contract in 2.01, because CE itself is excluded. Use normal platform window controls and resize behavior.

Small and high-DPI desktops remain supported: each Options page scrolls within the dialog so controls remain reachable. Closing the owning workspace closes an active Options dialog without publishing its staged values. See [General Options](help:IDH_OPTIONS_GENERAL) and [Introduction](help:IDH_OPTIONS_INTRO).
