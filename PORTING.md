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
38. Qt file I/O treats a genuinely absent `colkanji.lst` as an optional empty
    list, rejects malformed files and dangling links, and serializes the full
    canonical list before atomically replacing the destination.
39. Kanji display policy persists as one versioned Qt settings record with
    strict canonical mode, color, and toggle fields. Corrupt records fail as a
    unit instead of silently coercing or mixing individual options.
40. The native application loads kanji display policy and optional
    `colkanji.lst` from its XDG configuration directory into candidate state
    before publication. JWP rendering refreshes colors after edits, history,
    conversion, formatting, code-page changes, and rejected-edit restoration;
    plain documents clear the overlay. Malformed reloads retain the prior
    working state, and malformed startup configuration fails noninteractively.
41. Native Kanji Color Options edits screen list mode, list color, and uncommon
    coloring as one validated policy update. The complete strict settings
    record is persisted before publication; failed persistence restores the
    prior policy and both persistent/transient editor overlays without ending
    an active WNN conversion.
42. Native kanji color-list management reproduces legacy Make, Append,
    Add/Remove, View, and Clear commands. Mutations prepare display state and
    atomically persist `colkanji.lst` before publishing the new list; View
    validates a complete unnamed JWP document before replacing the live editor.
    All commands remain inert during active WNN conversion.
43. A portable ordered `user.cnv` model parses and writes exact high-bit
    hiragana stems, inflection endings, slash-delimited JWP candidates, and LF
    records. It reconstructs full readings for editing, preserves imported
    multi-candidate inflected records, enforces all allocation budgets before
    growth, and exposes unsorted lookup records without a system index.
44. Each WNN conversion session owns a snapshot of optional user records.
    Manual and automatic lookup search those records after system data, and
    longer user keys participate in automatic conversion's wait decision
    without borrowing caller-owned storage.
45. Qt file I/O treats a genuinely absent `user.cnv` as an optional empty user
    dictionary, distinguishes other open failures from absence, validates
    existing bytes through the portable codec, and serializes the complete
    canonical payload before atomically replacing the destination.
46. Native WNN resource loading owns the editable user dictionary and gives
    each conversion session its lookup snapshot. The application loads
    `user.cnv` beside `user.sel` in its XDG data directory; malformed reloads
    leave the previous dictionary, preferences, and session working together.
47. Whole-dictionary replacement resolves pending kana conversion, constructs a
    complete replacement resource/session bundle, atomically writes `user.cnv`,
    and swaps only after success. Active conversion and failed persistence
    retain the previous live lookup snapshot and persisted dictionary.
48. New editable WNN entries use a strict portable factory separate from the
    permissive import codec. It derives all nine godan endings, validates the
    recovered ichidan i/e stem set and i-adjective endings, rejects multiple
    inflected candidates, and strips a matching kana suffix exactly once.
49. Explicit user-dictionary sorting reproduces the recovered display-row
    ordering, including slash-separated candidates and bracketed inflections,
    with the source-shaped selection algorithm rather than assuming a strict
    comparator. A comparison-step budget fails pathological sorts locally
    without limiting dictionary loading, lookup, or unsorted source order.
50. Native user-dictionary editing is split at a modeless working-copy dialog
    boundary. Add, Edit, Delete, Move, Sort, and Import remain local until Save
    publishes one complete dictionary; Cancel and failed saves publish nothing.
    Existing imported inflection records round-trip without normalization, and
    document insertion is an injected callback rather than hidden editor state.
51. MainWindow owns at most one modeless user-dictionary editor for the current
    WNN resource generation. Successful resource reloads discard stale working
    copies; Save atomically replaces the live `user.cnv` bundle, and Insert to
    File writes the exact legacy display row into a JWP selection as one
    portable-history transaction. Plain text and active conversion reject
    insertion without mutation.
52. EDICT record parsing owns the exact source bytes and exposes bounded
    Unicode records with byte spans, multiple readings, and slash definitions.
    It handles the four recovered newline forms, UTF-8 BOMs, strict UTF-8 and
    EUC-JP, and the exact 77-sequence JIS X 0212 subset converted by JWPxp.
    Parsing uses the first formal space-slash delimiter so malformed glosses
    cannot be reinterpreted as headword text or bypass definition budgets.
53. The portable JDX reader binds an index to its exact dictionary size,
    decodes every post-header word as an explicit one-based little-endian
    offset, validates character and record boundaries, and preserves the wire
    representation exactly. Lookup reproduces JINDEX ASCII and katakana
    folding plus UTF extension-page selection through a bounded linear scan;
    correctness never depends on trusting attacker-controlled physical sort
    order. Both the recovered source-size header and official size-plus-15
    header variant are accepted, and a four-byte header-only index is valid.
54. Portable direct EDICT search bounds input to the recovered 100-token limit,
    rejects short ASCII and mixed ASCII/Japanese keys, normalizes ASCII case and
    katakana, and carries exact matched source-byte spans from JDX lookup. It
    reproduces normal and Full ASCII plus Japanese beginning/end boundaries,
    handles the recovered three-byte JIS X 0212 subset, preserves repeated
    indexed occurrences, and rejects an index bound to different source bytes.
55. Portable EDICT name/place filtering reproduces the recovered definition-level
    behavior for old ENAMDICT tags: `s`, `u`, `g`, `f`, and `m` identify personal
    names, while `p` identifies places. A sole rejected type removes its whole
    definition; rejected tags are stripped from mixed recognized groups, and a
    record is rejected only when no definitions remain. Filtering preserves the
    record's source span, headword, readings, and original value.
56. Portable non-pattern adaptive deinflection reproduces the recovered ordered
    truncation, append, and replacement sequence without owning dictionary or UI
    state. It preserves naturally repeated queries, canonicalizes high-bit kana,
    covers every recovered godan ending, permits the 101-token temporary
    candidate from a 100-token query, and enforces explicit result/work budgets.
    Pattern queries fail explicitly until the separate pattern engine is ported.
57. Portable EDICT orchestration runs direct search first, then source-ordered
    adaptive passes under the recovered Always and Keep Searching policies. All
    sibling ending variants in one truncation pass run before accepted filtered
    matches decide whether another pass runs. Distinct index occurrences remain
    distinct, and global limits account for attempted queries, raw candidates,
    accepted results, and actual index-comparison work. The public direct API
    retains its 100-token boundary; only the private adaptive path may use a
    generated 101-token temporary key.
58. Portable EDICT pattern preprocessing reproduces the recovered 100-token
    input bound, ASCII/kana/fullwidth normalization, exact version-entry
    markers, fixed `to ` prefix, first-kanji anchor split, closing-bracket
    truncation, kana trailing-star shortcut, and explicit ASCII-boundary and
    adaptive-disable state. Malformed or anchorless wildcard plans fail before
    dictionary lookup.
59. Record-local EDICT wildcard matching validates an indexed anchor against its
    parsed dictionary record, expands exact source-byte spans through prefix and
    postfix patterns, preserves the recovered EUC-versus-UTF literal behavior,
    and treats recovered JIS X 0212 sequences as one character. One explicit
    budget covers record decoding, boundary and anchor validation, assertions,
    and greedy-star backtracking; forged plans cannot exceed the source input
    bound or escape the selected record.
60. Pattern orchestration counts every indexed anchor occurrence, composes
    record-expansion work with the global index-work budget, applies configured
    ASCII or Japanese boundaries to the expanded source-byte span, and filters
    names before per-query and global accepted-result limits. UTF extension-page
    interpretation comes from the source-bound index rather than mutable caller
    state.
61. Portable kanji contingent planning validates the recovered caller
    preconditions and transforms an eligible query into either `*<query>*` or
    `[<query>*`. It reproduces the first-kanji anchor split while rejecting
    truncated, oversized, non-Japanese, kanji-free, and pattern-bearing inputs
    before wildcard execution.
62. EDICT contingent orchestration reproduces source eligibility, forced mode,
    adaptive likely-ending selectivity, honorific-prefix exact retry, kanji
    wildcard expansion, long-key and kana boundary-relaxed retries, and forced
    personal/place rejection. Direct, contingent, and following adaptive stages
    share query, raw-candidate, accepted-result, and actual index/matcher work
    budgets; continuation depends on accepted filtered results.
63. The portable dictionary registry codec reads and writes the recovered binary
    `dict.cfg` format in both ANSI-byte and UTF-16LE wire variants. It preserves
    ordered flag/name/path triples, every format/name/special/runtime flag,
    duplicate entries, and source-valid empty fields while enforcing exact
    terminators, surrogate validity, canonical flag order, and explicit budgets.

The legacy codecs intentionally reject JIS X 0201 halfwidth kana, JIS X 0212,
vendor extensions, malformed byte sequences, unassigned table cells, and
Unicode characters outside the recovered repertoire. Supporting any of these
requires a separate fixture-backed change rather than silent substitution.

## Next slices

1. Load ordered EDICT dictionary resources from the portable registry, then add
   native result-view slices.
2. Port paged printing and clipboard color policy over the rich JWP document
   layout.
3. Port configuration, remaining import/export paths, and help.

Each slice is committed independently after focused tests and the complete
CTest suite pass.
