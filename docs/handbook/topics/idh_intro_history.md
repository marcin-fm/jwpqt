# Historical Development

JWPqt 2.01 continues a line that began with JWP, then JWPce, and later JWPxp. The original interface and Japanese word-processing ideas were associated with Stephen Chung's JWP; JWPce was a substantial rewrite by Glenn Rosenthal, initially shaped by small-screen Windows CE constraints. JWPxp extended that desktop lineage. JWPqt preserves this history as compatibility context rather than pretending that a modern Qt application is the old program unchanged.

The 2.01 port recovered and audited the JWPxp 1.67 command, document-format, settings, printing, help, and installation inventory. Portable C++ owns the compatibility-sensitive core, while Qt 6 supplies the application window, widgets, filesystem integration, native input handling, and platform packaging. Unicode is used by the native interface, but original JIS values remain explicit at JWP compatibility boundaries so that legacy document and lookup behavior is not casually guessed.

The historical source tree remains preserved by the `jwpxp-1.67` release tag; obsolete Win32 implementation files are deliberately not copied into the native tree. Where a legacy feature depends on a Windows-only contract, JWPqt either implements the user result portably or documents the exclusion. CMake/CPack replaces registry installation, the embedded handbook replaces WinHelp, portable Qt MIME formats replace private clipboard formats, and WebAssembly uses browser-owned files rather than local path ownership.

This approach keeps relevant concepts such as Japanese editing, WNN conversion, JWP documents, and kanji lookup while making platform differences explicit. See [Advanced Options](help:IDH_OPTIONS_ADVANCED) and [Support](help:IDH_SUPPORT_GENERAL).
