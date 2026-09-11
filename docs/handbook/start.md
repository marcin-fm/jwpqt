# jwpqt Handbook

jwpqt is a native Qt 6 Japanese word processor for Linux, Windows, macOS, and
WebAssembly, ported from JWPxp 1.67 and JWPce. It uses Qt 6, not Wine. The
recovered user-visible commands have native
implementations or documented platform replacements and source-justified
exclusions. This handbook describes their native behavior and those explicit
platform differences.

## First Session

New creates a Japanese-editing document. Type romaji in Kanji mode to compose
kana, then Convert to choose a kanji candidate when WNN data is installed.
F4 switches the local input mode. The status controls select Kanji, ASCII or
full-width JASCII, and Insert or Overwrite. A selected range is replaced without
consuming the following text.

Save chooses the document format. A file's storage encoding is independent of
its editing mode. New Plain-Text Document starts as UTF-8 text with Japanese input
available. Turn off Input > Japanese Editing when unrestricted Unicode outside
the original JWP repertoire is required; the storage format remains plain text.
Lossy conversions require confirmation.

## Topics

- [Documents and projects](files.md)
- [Japanese editing and Find/Replace](editing.md)
- [Dictionary search](dictionary.md)
- [Kanji information and lookup](kanji.md)
- [Printing and PDF](printing.md)
- [Settings and histories](settings.md)
- [Installation and optional runtime data](installation.md)
- [About and licensing](about.md)

Press F1 in the editor or a tool to open a relevant topic. The search field
searches all bundled topics, not private documents or dictionary data. Back,
Forward, Contents and zoom controls work without a network connection.

Missing dictionaries do not prevent ordinary editing. Help > Runtime Resources
reports which optional components are loaded and explains failures. See the
installation topic before provisioning separately licensed data.
