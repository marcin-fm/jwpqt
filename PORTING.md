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
8. The current slice adds checked paragraph/document mutation with the legacy
   split, join, formatting, and hard-page-break semantics.

The legacy codecs intentionally reject JIS X 0201 halfwidth kana, JIS X 0212,
vendor extensions, malformed byte sequences, unassigned table cells, and
Unicode characters outside the recovered repertoire. Supporting any of these
requires a separate fixture-backed change rather than silent substitution.

## Next slices

1. Port undo/redo, selections, and search/replace over the portable document
   model.
2. Integrate JWP documents with native open/save while preserving metadata and
   the selected CP1250 through CP1258 extension table.
3. Port kana-to-kanji conversion and dictionary lookup with fixtures captured
   from the recovered implementation.
4. Replace `QPlainTextEdit` scaffolding with a custom Qt editor surface once the
   paragraph model can drive wrapping, selection, conversion spans, and kanji
   coloring.
5. Port lookup tools, configuration, and printing as separate vertical slices.

Each slice is committed independently after focused tests and the complete
CTest suite pass.
