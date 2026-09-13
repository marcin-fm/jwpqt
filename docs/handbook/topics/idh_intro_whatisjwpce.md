# What is JWPce?

JWPqt 2.02 is the native Qt 6 continuation of the JWPce and JWPxp Japanese word-processor lineage. It runs as one application on Linux, Windows, macOS, and WebAssembly; it is not a Wine wrapper and does not run the old Windows executable. Its purpose remains practical Japanese reading and writing, especially where a document editor, Japanese input, dictionary search, and kanji reference tools should work together.

Create a Japanese document to use the original JWP-compatible editing model, paragraph formatting, hard page breaks, and JWP/JCE saving. Create a plain-text document when a conventional Unicode text file is more appropriate. Both can use Japanese input; turn off Japanese Editing only when a plain-text document needs characters outside the original JWP repertoire. Storage format and editing mode are separate decisions.

Type romaji in Kanji mode to compose kana. With optional WNN data installed, Convert offers kana-to-kanji candidates. The program also provides Japanese-English dictionary search, Character Information, radical and other kanji lookup methods, Find and Replace, projects, session restoration, printing and PDF preview on desktop ports, and configurable fonts, colors, and toolbar layout.

The Web port has the same Qt interface but uses browser upload/download and IndexedDB. Browser security prevents referenced-file projects, and Qt WebAssembly has no PrintSupport, so project and printing commands are unavailable there. Missing optional dictionaries do not prevent editing or use of this offline help. See [Input Modes](help:IDH_TEXT_INPUTMODES) and [Dictionary Introduction](help:IDH_DICT_GENERAL).
