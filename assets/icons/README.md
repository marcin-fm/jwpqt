# Vector interface artwork

These standard SVG files are the native JWPqt interface artwork. The toolbar
and lookup diagrams use transparent, palette-neutral black shapes. At runtime
Qt renders them at the requested logical size and applies the active or disabled
text color, so the same source remains legible in light and dark themes. The
application icon intentionally keeps its own blue and white colors.

The toolbar has one named icon for every command in the 36-command customization
catalog. `skip-diagram.svg` and `four-corner-diagram.svg` preserve the category
order and shapes of the recovered lookup legends as clean scalable geometry;
they are not automatic bitmap traces. `kana.svg` uses the `あ` outline and the
application icon restores the original `愛` identity. Their glyph outlines were
generated from Noto Sans CJK JP 2.004, released under the SIL Open Font License
1.1, then embedded as font-independent SVG paths.

The recovered legacy BMP and ICO files remain at the repository root as source
provenance, but the native runtime does not load `toolbar.bmp`, `skiptype.bmp`,
`fourcorners.bmp` or `mainicon.ico`. `hsradicals.bmp` and externally provisioned
radical artwork remain raster resources until the radical glyph set is ported.
