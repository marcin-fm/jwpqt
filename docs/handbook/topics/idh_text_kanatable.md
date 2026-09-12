# Kana Table (Legacy)

The legacy JWP Kana Table topic describes a feature that is not implemented as a separate native JWPqt command. For explicit kana entry, use the JIS Table or Kanji-mode romaji composition; for existing romaji, use selected-romaji conversion. The JIS Table and conversion paths validate the active destination before inserting or replacing text.

JIS Table insertion places the selected character in the active document, replacing its current selection in one undoable edit. It can be used in native Japanese Editing or unrestricted Unicode editing, subject to the destination's normal validation. JWPqt does not silently substitute unsupported text, split a surrogate pair, or insert into a read-only document or an active conversion preview. Finish pending kana or accept a candidate first so the destination remains unambiguous.

The JIS Table is useful for a kana or kanji known visually or by its JIS position. It does not change the current input mode. Select **Kanji** for ongoing romaji composition, **ASCII** for literal text, or **JASCII** for full-width mapped text as described in [Input Modes](help:IDH_TEXT_INPUTMODES). A character entered from the table remains ordinary document text and may be selected for explicit conversion.

For automated kana entry, Kanji mode composes lowercase romaji as hiragana and uppercase syllables as katakana. Select existing printable romaji and use `F2` or `Ctrl+>` to replay it into kana without WNN data. A second Convert can request candidates if WNN resources are loaded. See [Entering Hiragana](help:IDH_TEXT_HIRAGANA), [Entering Katakana](help:IDH_TEXT_KATAKANA), and [Explicit Kanji Conversion](help:IDH_TEXT_EXPLICITKANJI).
