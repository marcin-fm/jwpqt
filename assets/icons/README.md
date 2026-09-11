# Vector interface artwork

These standard SVG files are the native JWPqt interface artwork. The toolbar
and lookup diagrams use transparent, palette-neutral black shapes. At runtime
Qt renders them at the requested logical size and applies the active or disabled
text color, so the same source remains legible in light and dark themes. The
application icon intentionally keeps its own blue, white and cyan colors.

The toolbar has one named icon for every command in the 36-command customization
catalog. `skip-diagram.svg` and `four-corner-diagram.svg` redraw the semantic
lookup legends as scalable geometry; they are not automatic bitmap traces.

The recovered legacy BMP and ICO files remain at the repository root as source
provenance, but the native runtime does not load `toolbar.bmp`, `skiptype.bmp`,
`fourcorners.bmp` or `mainicon.ico`. `hsradicals.bmp` and externally provisioned
radical artwork remain raster resources until the radical glyph set is ported.
