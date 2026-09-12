# Character Placement and the Tab Key

Tab is a document navigation and character-placement key in the native editor, not a request to move focus away from an active conversion. Before navigating, JWPqt accepts any active WNN candidate, keeping the conversion transaction at its original source. Tabs within imported or selected romaji are also permitted by explicit conversion: the selection may contain printable ASCII or tabs, must stay in one paragraph, and is converted without changing text outside the selected range.

JWPqt's native layout retains fixed Japanese cells and one-cell tab stops for printing. The printing fit pass applies next-cell ASCII padding before tabs where eligible, while native Japanese output keeps its tab stops even when ASCII padding is off. This concerns layout only; it does not change the actual tab character into spaces or alter the source text. Unicode-only documents use Qt layout.

Input mode determines what ordinary typed characters mean, while Tab retains its document role. In Kanji mode, romaji composition is settled before movement. ASCII mode inserts direct text; JASCII inserts full-width mapped characters. Press `Insert` or use **Edit > Input Mode > Overwrite Mode** to change typing behavior, but paste and lookup insertion remain insertion operations rather than consuming text after a tab or caret.

Use paragraph formatting when alignment should be expressed as indent, hanging indent, or spacing rather than a run of tabs. Those values survive native formatting and printing. A hard page break is structural and is inserted with `Ctrl+Return`; it is not produced by Tab. The visual placement of a tab can vary with [Text Display and Line Width](help:IDH_EDIT_WIDTH), page layout, and font presentation, but its character identity remains intact for selection, search, copy, and Undo.

See [Formatting Paragraphs](help:IDH_EDIT_PARAGRAPH), [Explicit Kanji Conversion](help:IDH_TEXT_EXPLICITKANJI), and [Using the Clipboard](help:IDH_EDIT_CLIPBOARD).
