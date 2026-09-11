# Vector interface artwork

These standard SVG files are the native JWPqt interface artwork. The toolbar
and lookup diagrams use transparent, palette-neutral black shapes. Toolbar icons
are materialized at their supported icon sizes. The SKIP and Four Corner widgets
instead paint their SVG paths directly into the current layout and device scale,
without a fixed intermediate pixmap. Runtime palette colors keep both forms
legible in light and dark themes. The application icon intentionally keeps its
own blue and white colors.

The toolbar has one named icon for every command in the 36-command customization
catalog. `skip-diagram.svg` and `four-corner-diagram.svg` preserve the category
order and shapes of the recovered lookup legends as clean scalable geometry;
they are not automatic bitmap traces. `kana.svg` uses the `あ` outline and the
application icon restores the original `愛` identity. Their glyph outlines were
generated from Noto Sans CJK JP 2.004, released under the SIL Open Font License
1.1, then embedded as font-independent SVG paths.

Obsolete legacy BMP and ICO files are available from the `jwpxp-1.67` tag, not
the native working tree. The two radical sheets still consumed by native lookup
live under `assets/data/`; externally provisioned radical metadata remains
raster data until that glyph set is ported.
