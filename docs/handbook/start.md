# JWPqt 2.00 Handbook

This offline handbook ports the complete 125-topic JWPce help inventory to the
native Qt 6 application. It preserves the original organization while replacing
obsolete Win32 and Windows CE procedures with the corresponding Linux, Windows,
macOS, and WebAssembly behavior.

## First Session

New creates a Japanese-editing document. Type romaji in Kanji mode to compose
kana, then Convert to choose a kanji candidate. F4 switches the local input mode.
The status controls select Kanji, ASCII or full-width JASCII, and Insert or
Overwrite. A selected range is replaced without consuming the following text.

Save chooses the document format. A file's storage encoding is independent of
its editing mode. New Plain-Text Document starts as UTF-8 text with Japanese
input available. Turn off Input > Japanese Editing only when unrestricted
Unicode outside the original JWP repertoire is required.

## Chapters

- [Introduction](help:IDH_INTRO_WHATISJWPCE)
- [Installation](help:IDH_INSTALL_INSTALL)
- [JWPce Interface](help:IDH_INTERFACE_MAIN)
- [Entering Text](help:IDH_TEXT_EDITMODES)
- [Basic Editing](help:IDH_EDIT_MOVING)
- [Working with Kanji](help:IDH_KANJI_CHARINFO)
- [Dictionary](help:IDH_DICT_GENERAL)
- [Working with Files](help:IDH_FILE_TYPES)
- [Printing](help:IDH_PRINT_GENERAL)
- [Options and Settings](help:IDH_OPTIONS_INTRO)
- [Fonts](help:IDH_FONTS_INTRO)
- [Support](help:IDH_SUPPORT_GENERAL)

Use the chapter/topic list or search field to navigate all 125 articles. Search
covers the bundled handbook, not private documents or dictionary data. Press F1
in the editor or an application dialog to open its exact relevant topic. Back,
Forward, Contents and zoom controls work without a network connection.

Missing dictionaries do not prevent ordinary editing. Help > Runtime Resources
reports which optional components are loaded and explains failures.
