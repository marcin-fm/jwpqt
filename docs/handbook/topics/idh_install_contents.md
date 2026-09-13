# What Should be in the Distribution?

A normal JWPqt 2.03 package contains the native application and an offline handbook. Linux packages additionally include the desktop entry, scalable icon, MIME definitions, retained licenses and notices, and normally the original lookup payloads under the application data directory. Windows ZIP and macOS DMG packages carry their platform-native application assets and deployed Qt runtime support. The Web static-site package includes the Web application files, handbook, notices, embedded Noto Sans JP and its OFL, and the payload archive used by the virtual filesystem.

Normal data-bearing releases include recovered EDICT, ENAMDICT, kanji information, radical, stroke, and related data as separate unmodified files. The built-in WNN system dictionary is part of every build. These payloads have their own original notices and can impose noncommercial or permission conditions. They are not GPL merely because the executable is GPL. A data-free edition, configured with `-DJWPQT_BUNDLE_LEGACY_DATA=OFF`, intentionally omits the separately licensed legacy payloads.

Do not expect `jwpce.exe`, `UPDATE.EXE`, `.hlp`/`.cnt` WinHelp files, registry scripts, CE processor binaries, or legacy bitmap fonts to be required distribution contents. JWPqt uses a Qt executable or platform bundle, CMake/CPack delivery, and an embedded handbook. Legacy `.f00` files can be configured privately when supplied by the user, but are not globally installed or silently bundled.

Check the installed `RELEASE_NOTES.md`, GPL, and original notices for artifact names and licensing details. See [Support](help:IDH_SUPPORT_GENERAL) and [File Types](help:IDH_FILE_TYPES).
