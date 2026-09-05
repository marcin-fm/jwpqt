# Porting jwpqt

jwpqt is replacing the JWPxp 1.67 Win32 application incrementally. The five
recovered JWPce/JWPxp release commits and their tags remain the historical base;
native Linux work begins after `jwpxp-1.67`.

## Boundaries

- `src/core` is portable C++ with no Qt, Win32, or operating-system APIs.
- `src/qt` owns the Qt application, filesystem integration, and widgets.
- The historical root sources remain the behavioral and format reference while
  individual responsibilities move behind tested portable interfaces.
- The native application does not depend on Wine, Winelib, or PE execution.

The legacy program stores Japanese characters as 16-bit JIS row/cell values.
That representation remains explicit at compatibility boundaries. Unicode is
used for the native UI, not as an untested replacement for dictionary keys or
legacy file structures.

## Completed slices

1. `980db33` adds the Qt 6 application shell, a strict UTF-8 core, atomic file
   replacement through `QSaveFile`, and a basic open/edit/save path.
2. `4a50fbe` extracts reversible JIS X 0208 pair transformations for EUC-JP
   and Shift-JIS from the legacy conversion code.
3. `3d9a5f1` maps the 6,892-character legacy JIS repertoire to and from Unicode
   using the recovered JWP mapping data and canonical duplicate ordering.
4. `883bd41` adds strict full-text EUC-JP and Shift-JIS codecs over those
   primitives.
5. `b1fa787` parses and canonically writes the portable B1, B2, and J1.20 JWP
   document containers.
6. `9e9ce3e` maps the recovered CP1250 through CP1258 extension tables.
7. `c7fcf41` bridges complete JWP text-token streams to and from Unicode while
   preserving legacy canonicalization.
8. `ce1ac01` adds checked paragraph/document mutation with the legacy split,
   join, formatting, and hard-page-break semantics.
9. `e8d9236` adds bounded document transactions, undo/redo, caret restoration,
   and legacy typing/deletion coalescing.
10. `95e2937` adds paragraph-local forward/backward search, legacy ASCII/JASCII
    comparison, and strongly exception-safe replacement.
11. `b44feb9` maps flat Unicode edits to checked paragraph mutations while
    preserving formatting and protecting structural hard page breaks.
12. `984829a` adds atomic native file I/O for complete JWP containers.
13. `74d39dc` names and parses the recovered CP1250 through CP1258 tables.
14. `8258437` opens, edits, and saves JWP documents in the native Qt window
    while preserving metadata, paragraph formatting, and hard page breaks. Its
    explicit code-page selector can be set before a file is opened.
15. `4c5d529` exposes Find, Find Next, and Find Previous for both plain text and
    JWP documents, including legacy ASCII/JASCII comparison and bounded wrap.
16. `748d9fb` enumerates non-overlapping document matches independently of
    cursor direction and wrapping for deterministic whole-document operations.
17. `dbf1599` exposes Replace Next and Replace All natively. JWP
    replacements preserve structure and metadata, and each accepted occurrence
    remains independently undoable as in JWPxp.
18. `1ef8e06` routes JWP edits through portable transaction history.
    Undo and redo restore the complete JWP document and caret, consecutive
    typing/deletion coalesces, Replace All retains one entry per occurrence,
    saves retain history, and both menu and context-menu actions use it.
19. `2f56f44` extracts the recovered desktop romaji-to-kana state machine,
    including direct, compound, symbol, case, pending, and consonant behavior.
20. `07c0f82` parses and validates the recovered WNN index/data wire formats
    without native-ABI assumptions or bundled dictionary payloads.
21. `1de61c1` reproduces ordered WNN exact, special-stem, and conjugated
    candidate lookup, suffix attachment, extension checks, and stable duplicate
    removal over the portable dictionary model.
22. `90e1393` parses and writes the recovered 8-byte `user.sel` records,
    restores valid candidate offsets, and reproduces the fixed-capacity learning
    and eviction behavior without native structs.
23. `3e6ccc4` optionally loads `user.sel`, preserves partial legacy files
    through the portable parser, and atomically replaces changed preferences
    without clearing dirty state on failure.
24. `44de037` starts WNN conversion from stored preferences, repairs stale
    offsets, wraps candidate cycling in both directions, learns choices, and
    terminates explicitly through accept or cancel.
25. `15d485e` replaces selected JWP kana with WNN candidates, tracks
    variable-length selections and caret direction, groups all cycling into one
    undo entry, and rolls back safely on controller failure.
26. `967dd64` makes the native Qt window load explicit WNN resources, convert
    same-paragraph JWP selections, cycle candidates with Space or Shift+Space,
    accept the displayed candidate with Enter or Escape, and atomically persist
    learned preferences under an explicit or XDG user-data path.
27. `733d955` makes the native JWP editor expose an explicit Kana Input mode
    that sends
    printable desktop romaji through the portable composer, inserts recovered
    hiragana and katakana tokens through synchronized document history, and
    resolves or discards pending composition at command and document boundaries.
28. `580ad62` makes the portable WNN session reproduce automatic conversion
    policy: wait while
    a complete key can grow, convert a terminal complete key, or back off to the
    longest valid prefix. Prepared prefix transactions retain a caret after the
    unconverted suffix while candidate lengths change.
29. The native Kana Input adapter tracks recoverable conversion spans without
    hijacking the real text selection, waits for extendable WNN keys, applies
    terminal or longest-prefix candidates, preserves unmatched suffixes and
    caret placement, and invalidates stale spans on external edits.
30. A native `JwpEditor` maps portable paragraph indents and proportional line
    spacing to Qt rich-document layout, displays hard page breaks in the
    continuously scrolling editor, and carries those breaks into paged document
    layout for printing. Main-window restoration paths reapply or clear the
    presentation without changing portable JWP history.
31. Portable paragraph-format mutation validates the legacy persisted field
    bounds and left-margin relationship before atomically applying left, right,
    first-line, and proportional-spacing values to an inclusive paragraph
    range. Page-width-dependent validation remains a native command concern.
32. The native Format Paragraph command applies one format to the caret
    paragraph or inclusive selected paragraphs as one portable undo entry,
    validates margins against the visible dynamic character width, preserves
    selection and hard breaks, and updates rich layout without rewriting text.
33. Portable hard-page-break insertion reproduces the legacy empty,
    paragraph-start, paragraph-middle, paragraph-end, and existing-break
    structures with copied formatting, exact following-caret placement, and
    candidate-document publication for strong failure safety.
34. The native Ctrl+Enter command deletes any selection and inserts a hard page
    break as one portable history transaction, restores the following caret and
    rich layout, and remains unavailable during active WNN conversion or in
    plain-text documents.
35. A portable kanji-color policy reproduces the raw JWP token thresholds, the
    list-member/non-member precedence over uncommon coloring, the historical
    fixed-list index arithmetic, and Windows `COLORREF` byte/fallback decoding
    without coupling those decisions to Qt or persisted list storage.
36. A portable fixed-capacity kanji-color list parses ordered or unordered
    canonical EUC pairs, deduplicates membership, writes sorted canonical
    `colkanji.lst` bytes, and can append raw kanji tokens from a JWP document.
    Truncated, non-EUC, and out-of-capacity records fail loudly.
37. The rich Qt editor renders kanji colors as a non-document
    `ExtraSelection` layer, preserving text, block formats, selection,
    modified state, and undo history. A separate later transient layer keeps
    WNN candidate highlighting from erasing persistent kanji colors.

The legacy codecs intentionally reject JIS X 0201 halfwidth kana, JIS X 0212,
vendor extensions, malformed byte sequences, unassigned table cells, and
Unicode characters outside the recovered repertoire. Supporting any of these
requires a separate fixture-backed change rather than silent substitution.

## Next slices

1. Port document-backed kanji coloring and paged printing over the rich JWP
   document layout.
2. Port editable WNN user dictionaries and lookup tools as separate vertical
   slices.
3. Port configuration, remaining import/export paths, and help.

Each slice is committed independently after focused tests and the complete
CTest suite pass.
