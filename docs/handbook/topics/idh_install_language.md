# International Support

JWPqt presents a native Qt 6 interface on Linux, Windows, macOS, and WebAssembly. It supports Japanese document text, Japanese query fields, romaji-to-kana composition, WNN kana-to-kanji conversion when data is available, and multiple Japanese file encodings. The application uses Unicode in its interface while retaining explicit JIS compatibility at JWP document and lookup boundaries.

The historical `JWPCE_LANG.DLL` localization mechanism is not part of JWPqt 2.02. It depended on a version-matched Windows interface DLL and startup-only string replacement. The native source does not claim a plug-in translation format or runtime language-switch feature in its place, so users should not copy legacy language DLLs into a JWPqt installation. Use the operating system and Qt-supported environment for normal desktop language behavior.

Japanese font availability is platform dependent. Linux desktop users should install a suitable Japanese font, such as Noto Sans CJK JP, through their normal distribution. The Web build embeds Noto Sans JP because a browser cannot rely on arbitrary host fonts. Font choices for File, List, Table, Print, and other Japanese roles are configurable in Tools > Options; unavailable families fall back with a diagnostic rather than silently pretending to load.

For documents, choose an explicit encoding when automatic detection cannot establish the correct format. JWPqt supports UTF-8, UTF-7, UTF-16LE/BE, JFC, EUC-JP, Shift-JIS, New/Old/NEC JIS, and legacy code pages. See [Japanese Encoding Systems](help:IDH_FILE_ENCODING) and [Installing Additional Fonts](help:IDH_FONTS_INSTALL).
