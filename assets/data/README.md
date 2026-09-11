# Native lookup data

These are the only recovered bitmap assets retained by the native application.

- `spahn-radicals.bmp` is embedded in the Qt lookup interface. Its historical
  source is `jwpxp-1.67:hsradicals.bmp`.
- `kanji-radicals.bmp` supplies the optional external kanji radical glyph sheet.
  Install it as `radicals.bmp` beside `kanjinfo.dat`. Its historical source is
  `jwpxp-1.67:radicals.bmp`.
- `wnn.dat` and `wnn.dix` are the official WNN kana-to-kanji system dictionary
  and index distributed with JWPxp 1.67. Every application target embeds them
  so Japanese conversion works from a clean profile. The WNN redistribution
  notice is retained in `docs/legal/original-notices.txt`.

The remaining recovered BMP and ICO files are available from the tag and are
not duplicated in the native tree.
