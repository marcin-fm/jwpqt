# Installation and Runtime Data

The Linux package contains the native executable, offline handbook, desktop
entry, icons, MIME definitions and original license notices. It does not bundle
Qt libraries or optional dictionary datasets. Install system Qt 6 Widgets,
PrintSupport and Svg plus a Japanese font such as Noto Sans CJK JP. A binary archive is
for a compatible Linux architecture and library ABI, not a universal AppImage.

## Paths

Default configuration and user data use Qt's application locations, normally
~/.config/jwpqt/jwpqt and ~/.local/share/jwpqt/jwpqt on Linux. Use
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

## Optional Data: User Provisioning

Acquire data from a source you are entitled to use and retain its notices.
Nothing is downloaded automatically. Keep backups and do not overwrite an
existing configuration or user dictionary while provisioning.

- WNN requires both wnn.dix and wnn.dat in the configuration directory or the
  explicit WNN directory. User learning and conversions use user.sel/user.cnv
  in the user-data directory.
- Japanese-English lookup uses the binary registry dict.cfg, whose ordered
  entries point to separately acquired EDICT-style files and any required .jdx
  indexes. The recovered JWPxp.dic registry can be copied as dict.cfg only after
  checking its relative paths and obtaining the referenced data. Do not replace
  an existing registry without a backup. user.dct is user-created.
- Character information uses kanjinfo.dat; radical/stroke tools use radical.dat,
  stroke.dat and radicals.bmp in the configuration directory.

Help > Runtime Resources reports successful counts, absent components and
errors. A zero exit from --resource-report does not mean every optional component
is present. A partial WNN pair or corrupt required configuration is an error.
The editor and this handbook do not require optional data.

## Installing and Removing

CMake supports `cmake --install BUILD --prefix PREFIX`, including DESTDIR staging.
The TGZ binary package can be extracted into a private prefix and run as
PREFIX/bin/jwpqt from any working directory. For desktop launch, install its
share/applications, share/icons and share/mime entries into corresponding XDG
locations, ensuring jwpqt is on PATH; refresh the desktop/MIME caches with your
distribution's normal tools. Installing a file type does not force a default
application. The application never registers itself in a Windows registry.

Build a binary TGZ with `cpack --config BUILD/CPackConfig.cmake`. Generate its
matching source archive with `cpack --config BUILD/CPackSourceConfig.cmake` from
the same source tree. When distributing binaries, supply the corresponding
source and retained notices as required by the GPL; a license file alone is
not a substitute for source availability. Source packages exclude Git metadata
and local assistant caches. Optional runtime data remains separately licensed.

Remove a private installation prefix or use the installation manifest/package
manager for a system install. User configuration and documents are separate and
must not be deleted as part of removing the application.

[Licenses and notices](about.md) | [Contents](start.md)
