# Choose Color Dialog Box (Windows CE)

The Windows CE-specific Choose Color dialog is not part of JWPqt because Windows CE is not a supported platform. JWPqt uses Qt's native color selection controls and validated color fields instead of reproducing a custom PocketPC palette dialog.

In Tools > Options > Colors, stage information-heading, kanji-list, and uncommon-kanji colors, list mode, and uncommon-character coloring. Enter a `#RRGGBB` value, choose a color through the native dialog, or select Inherit. Cancel leaves the current policy intact. Accepted changes update existing and inactive native documents without editing their text or undo history. Kanji Color Options updates the corresponding stored choices as well.

Source color compatibility is retained where meaningful. Explicit imported values take precedence; absent overrides leave the native INI color policy authoritative. Inherit returns to that policy. Special Windows palette references are preserved and disclosed instead of being mistaken for RGB. Unsupported heading/list references are disclosed, while uncommon-character color uses the recovered green fallback. Information headings adapt when needed for theme contrast.

Color kanji markings are display overlays, not changes to document characters. Uncommon-character dots appear in conversion and lookup bars without entering copied or inserted text. Persistent kanji list colors can also be applied to clipboard images or printing only when the corresponding options are enabled. See [Color-Kanji Options](help:IDH_KANJI_COLOROPTIONS) and [Display Options](help:IDH_OPTIONS_DISPLAY).
