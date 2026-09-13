# The GNU General Public License

JWPqt program source is licensed GPL-2.0-or-later. The license grants the freedoms associated with free software, including running, studying, modifying, and redistributing the program under its terms. A distribution of binaries must make corresponding source available as required by the GPL; a license file alone is not a substitute for source availability. The full text is included with the installed handbook as `gnugpl.txt` and is available from About.

The program license does not automatically relicense every file shipped beside it. Normal 2.03 packages may include recovered EDICT, ENAMDICT, KANJIDIC-derived, WNN, SKIP, frequency, radical, and stroke data as separate unmodified payloads. These materials retain their original notices, noncommercial conditions, permissions, and restrictions. They are not GPL program data merely because a package carries both. Read the included original notices before redistributing a data-bearing package, and retain them with the payloads.

Builders who need a GPL-only application package can configure with `-DJWPQT_BUNDLE_LEGACY_DATA=OFF`. That creates a data-free build; JWPqt does not download restricted data automatically. Optional lookup features may then be unavailable until valid, separately acquired resources are configured.

Qt itself is dynamically linked and separately licensed. Consult the Qt license supplied by the distribution. This overview does not replace the GPL or third-party notices, which control their own covered material. See [Support](help:IDH_SUPPORT_GENERAL) and [What is saved with a JWPce File?](help:IDH_FILE_CONTENTS).
