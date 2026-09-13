# Installation and Runtime Data

The normal Linux package is a noncommercial-data edition containing the native
executable, offline handbook, desktop entry, icons, MIME definitions, original
license notices and the JWPce EDICT, ENAMDICT and kanji/radical/stroke datasets
as separate unmodified payload files. It does not bundle Qt libraries. Install
system Qt 6 Widgets, PrintSupport and Svg plus a Japanese font such as Noto Sans CJK JP. A binary
archive is for a compatible Linux architecture and library ABI, not a universal
AppImage. The release workflow also builds a Windows ZIP with Qt runtime files,
a macOS DMG application bundle, and a WebAssembly static-site archive.
See the installed `RELEASE_NOTES.md` for the 2.01 artifact names and upgrade
notes.

The Web build embeds Noto Sans JP because WebAssembly cannot use arbitrary host
fonts. It includes the font's OFL, the program GPL, original notices, and a
matching source archive. GitHub Pages receives the same tested noncommercial-data
site from `master`; its separate `jwpqt.data` archive provides the payload files
to the Web virtual filesystem.

## Paths

Default configuration and user data use Qt's application locations, normally
~/.config/jwpqt/jwpqt and ~/.local/share/jwpqt/jwpqt on Linux. Windows and macOS
use their Qt standard application locations. The Web port uses browser IndexedDB
through an IDBFS-backed profile. Use
`jwpqt --resource-report` to see the actual resolved paths. Application-only
overrides avoid changing desktop theme configuration:

```
jwpqt --config-dir /path/to/config --user-data-dir /path/to/user-data
jwpqt --wnn-data-dir /path/to/wnn --resource-report
jwpqt --encoding utf-8 /path/to/document.txt
jwpqt --project /path/to/workspace.jpr
```

These explicit options replace JWPxp's first-argument `+directory` and
`-directory` network forms. The old forms divided several Win32 configuration
paths implicitly and the minus form hid resource errors; the native application
keeps each directory choice explicit and reports unavailable resources instead.

## Packaged and Replacement Data

The program is GPL-2.0-or-later. The packaged lookup datasets are not GPL: retain
the original notices, distribute them without financial return where required,
and obtain permission for uses reserved by their terms. Configure with
`-DJWPQT_BUNDLE_LEGACY_DATA=OFF` to produce a data-free build. Nothing is
downloaded automatically. Keep backups and do not overwrite an existing
configuration or user dictionary while provisioning replacement data.

- WNN requires both wnn.dix and wnn.dat in the configuration directory or the
  explicit WNN directory. User learning and conversions use user.sel/user.cnv
  in the user-data directory.
- Japanese-English lookup uses packaged EDICT/ENAMDICT and indexes when `dict.cfg`
  is absent. A user `dict.cfg` completely overrides that fallback; do not replace
  an existing registry without a backup. `user.dct` remains user-created.
- Character information and radical/stroke tools use the packaged data unless
  an explicit configuration resource is present. Explicit overrides are never
  silently mixed with packaged files.

Help > Runtime Resources reports successful counts, absent components and
errors. A zero exit from --resource-report does not mean every optional component
is present. A partial WNN pair or corrupt required configuration is an error.
The editor and this handbook do not require optional data.

## Installing and Removing

CMake supports `cmake --install BUILD --prefix PREFIX`, including DESTDIR staging.
The Linux TGZ can be extracted into a private prefix and run as
PREFIX/bin/jwpqt from any working directory. For desktop launch, install its
share/applications, share/icons and share/mime entries into corresponding XDG
locations, ensuring jwpqt is on PATH; refresh the desktop/MIME caches with your
distribution's normal tools. Installing a file type does not force a default
application. The application never registers itself in a Windows registry.

The Windows ZIP and macOS DMG are produced by their native hosted runners and
carry platform-native application icons. The Web archive is a static site:
serve or deploy its root without renaming `index.html`, `jwpqt.js`, `jwpqt.data`,
`jwpqt.wasm`, `qtloader.js`, or `qtlogo.svg`. GitHub Pages deployment is automated by
`.github/workflows/pages.yml`.

Build a binary TGZ with `cpack --config BUILD/CPackConfig.cmake`. Generate its
matching source archive with `cpack --config BUILD/CPackSourceConfig.cmake` from
the same source tree. When distributing binaries, supply the corresponding
source and retained notices as required by the GPL; a license file alone is
not a substitute for source availability. Source packages exclude Git metadata
and local assistant caches. Lookup payloads remain separately licensed even when
they travel in the same archive as the GPL program.

Remove a private installation prefix or use the installation manifest/package
manager for a system install. User configuration and documents are separate and
must not be deleted as part of removing the application.

[Licenses and notices](about.md) | [Contents](start.md)
