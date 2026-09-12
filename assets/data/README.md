# Native lookup data

These are the only recovered bitmap assets retained by the native application.

- `spahn-radicals.bmp` is embedded in the Qt lookup interface. Its historical
  source is `jwpxp-1.67:hsradicals.bmp`.
- `kanji-radicals.bmp` supplies the kanji radical glyph sheet and is installed as
  `radicals.bmp`. Its historical source is `jwpxp-1.67:radicals.bmp`.
- `wnn.dat` and `wnn.dix` are the official WNN kana-to-kanji system dictionary
  and index distributed with JWPxp 1.67. Every application target embeds them
  so Japanese conversion works from a clean profile. The WNN redistribution
  notice is retained in `docs/legal/original-notices.txt`.
- `edict`, `edict.jdx`, `enamdict`, `enamdict.jdx`, `kanjinfo.dat`, `radical.dat`
  and `stroke.dat` are unmodified files recovered from the official JWPxp 1.67
  package (SHA-256 `03458e21147b833336845942e32adb4a3d13987b27e51c33eb3971546112c9e1`).
  They are separate data payloads, not GPL application code. Their notices and
  noncommercial/permission restrictions are retained in
  `docs/legal/original-notices.txt`.

Normal packages include the restricted data as external files. Configure with
`-DJWPQT_BUNDLE_LEGACY_DATA=OFF` for a data-free build. Explicit configuration
resources override packaged defaults and are never silently mixed with them.

The remaining recovered BMP and ICO files are available from the tag and are
not duplicated in the native tree.
