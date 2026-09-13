# Porting jwpqt

jwpqt 2.03 is the audited native Qt 6 port of the JWPxp 1.67 Win32 application.
It was developed in incremental, tested slices. The six recovered JWPce/JWPxp
release commits and their tags remain the historical base; native port work
begins after `jwpxp-1.67` and now targets Linux, Windows, macOS, and WebAssembly.

## Boundaries

- `src/core` is portable C++ with no Qt, Win32, or operating-system APIs.
- `src/qt` owns the Qt application, filesystem integration, and widgets.
- The `jwpxp-1.67` tag preserves the complete historical behavioral and format
  reference; obsolete Win32 files are not duplicated in the native tree.
- The native application does not depend on Wine, Winelib, or PE execution.

The legacy program stores Japanese characters as 16-bit JIS row/cell values.
That representation remains explicit at compatibility boundaries. Unicode is
used for the native UI, not as an untested replacement for dictionary keys or
legacy file structures.

To inspect the original source without changing the current checkout:

```sh
git show jwpxp-1.67:jwpce.cpp
git archive --format=tar.gz --output=/srv/tmp/jwpxp-1.67.tar.gz jwpxp-1.67
```

## Implementation history

1. `47368ee59` adds the Qt 6 application shell, a strict UTF-8 core, atomic file
   replacement through `QSaveFile`, and a basic open/edit/save path.
2. `a3c268690` extracts reversible JIS X 0208 pair transformations for EUC-JP
   and Shift-JIS from the legacy conversion code.
3. `79be4daf0` maps the 6,892-character legacy JIS repertoire to and from Unicode
   using the recovered JWP mapping data and canonical duplicate ordering.
4. `82b5b4fd7` adds strict full-text EUC-JP and Shift-JIS codecs over those
   primitives.
5. `4422cc060` parses and canonically writes the portable B1, B2, and J1.20 JWP
   document containers.
6. `60aa74c07` maps the recovered CP1250 through CP1258 extension tables.
7. `c1cfcf03d` bridges complete JWP text-token streams to and from Unicode while
   preserving legacy canonicalization.
8. `2b482cbb1` adds checked paragraph/document mutation with the legacy split,
   join, formatting, and hard-page-break semantics.
9. `c907dd0c2` adds bounded document transactions, undo/redo, caret restoration,
   and legacy typing/deletion coalescing.
10. `a7ea317ea` adds paragraph-local forward/backward search, legacy ASCII/JASCII
    comparison, and strongly exception-safe replacement.
11. `f07b4552e` maps flat Unicode edits to checked paragraph mutations while
    preserving formatting and protecting structural hard page breaks.
12. `b34e665af` adds atomic native file I/O for complete JWP containers.
13. `d89e1b8a3` names and parses the recovered CP1250 through CP1258 tables.
14. `f937e994f` opens, edits, and saves JWP documents in the native Qt window
    while preserving metadata, paragraph formatting, and hard page breaks. Its
    explicit code-page selector can be set before a file is opened.
15. `3471bcc2a` exposes Find, Find Next, and Find Previous for both plain text and
    JWP documents, including legacy ASCII/JASCII comparison and bounded wrap.
16. `e981d3337` enumerates non-overlapping document matches independently of
    cursor direction and wrapping for deterministic whole-document operations.
17. `98bb426d9` exposes Replace Next and Replace All natively. JWP
    replacements preserve structure and metadata, and each accepted occurrence
    remains independently undoable as in JWPxp.
18. `484e16882` routes JWP edits through portable transaction history.
    Undo and redo restore the complete JWP document and caret, consecutive
    typing/deletion coalesces, Replace All retains one entry per occurrence,
    saves retain history, and both menu and context-menu actions use it.
19. `a1ddf2f2b` extracts the recovered desktop romaji-to-kana state machine,
    including direct, compound, symbol, case, pending, and consonant behavior.
20. `3a7c387b8` parses and validates the recovered WNN index/data wire formats
    without native-ABI assumptions or bundled dictionary payloads.
21. `6e01bb98f` reproduces ordered WNN exact, special-stem, and conjugated
    candidate lookup, suffix attachment, extension checks, and stable duplicate
    removal over the portable dictionary model.
22. `9b3a49966` parses and writes the recovered 8-byte `user.sel` records,
    restores valid candidate offsets, and reproduces the fixed-capacity learning
    and eviction behavior without native structs.
23. `192b03b7f` optionally loads `user.sel`, preserves partial legacy files
    through the portable parser, and atomically replaces changed preferences
    without clearing dirty state on failure.
24. `7ec0cf80f` starts WNN conversion from stored preferences, repairs stale
    offsets, wraps candidate cycling in both directions, learns choices, and
    terminates explicitly through accept or cancel.
25. `fa653cefa` replaces selected JWP kana with WNN candidates, tracks
    variable-length selections and caret direction, groups all cycling into one
    undo entry, and rolls back safely on controller failure.
26. `b63eaf95f` makes the native Qt window load explicit WNN resources, convert
    same-paragraph JWP selections, cycle candidates with Space or Shift+Space,
    accept the displayed candidate with Enter or Escape, and atomically persist
    learned preferences under an explicit or XDG user-data path.
27. `74ace15ee` makes the native JWP editor expose an explicit Kana Input mode
    that sends
    printable desktop romaji through the portable composer, inserts recovered
    hiragana and katakana tokens through synchronized document history, and
    resolves or discards pending composition at command and document boundaries.
28. `7240eed1b` makes the portable WNN session reproduce automatic conversion
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
    The modal editor validates before accepting, retains invalid values for an
    in-place retry, and leaves the document and its history unchanged on error.
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
    inflected candidates, and strips a matching kana suffix exactly once. The
    native entry editor additionally requires hiragana readings and keeps the
    fields and inflection open after every validation error, while imported
    source-compatible records remain accepted by the broader file boundary.
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
64. Mixed EDICT resources decode EUC-compatible headwords and readings while
    decoding meaning bytes independently through the selected CP1250-CP1258
    page, with the recovered unknown-page fallback to CP1252. Indexed and
    wildcard matching intentionally retains JINDEX's two-byte high-bit source
    stepping, including `0x8f`, while malformed unrepresentable headword pairs
    and embedded NULs fail explicitly.
65. The native `dict.cfg` file boundary treats only genuine absence as optional,
    parses every present file through the strict binary registry codec, and
    serializes complete canonical bytes before atomically replacing the target.
    Invalid limits, malformed files, dangling links, and unavailable output
    parents fail without publishing partial state or damaging an existing file.
66. Ordered EDICT resource loading preserves the complete registry while
    attempting searched entries in configuration order. ANSI-byte and UTF-16
    paths resolve explicitly against the configuration directory; configured
    EUC-JP, UTF-8, or mixed dictionaries are bounded and owned; JDX is required
    and source-bound only for indexed entries. Successful resources survive neighboring failures,
    quiet state is retained in diagnostics, blocking special files are rejected,
    and aggregate byte, structure, text, entry, attempt, and diagnostic budgets
    span both successful and malformed resources.
67. Unindexed EDICT resources use a bounded physical source-order anchor scan
    rather than a synthesized JDX. The first matching character start in each
    record yields one result before scanning advances to the next record,
    matching stops at record boundaries, and EUC-JP, UTF-8,
    recovered JIS X 0212, mixed high-byte pairs, and extension pages retain
    their established token semantics. Raw high-bit query tokens normalize like
    prepared searches, and exact attempted comparison work is reported under
    explicit result and work limits with constant extra memory.
68. Direct, adaptive, wildcard, and contingent orchestration now runs over
    either source-bound JDX lookup or the physical unindexed scan through the
    same filtering, boundary, continuation, result, candidate, query, and work
    limits. A multi-source collector runs each query across all configured
    backends before advancing to contingent or adaptive work, preserving global
    phase order. Linear wildcard expansion uses the dictionary-owned extension
    page.
69. Ordered resource dispatch searches normal dictionaries in configured order,
    routes indexed and unindexed resources through their matching backend, and
    optionally appends personal-name and place-name passes from the first names
    dictionary. Every direct query spans all normal resources before contingent
    or adaptive phases advance. One remaining-budget ledger spans every pass;
    non-keep files are reloaded for each search and failures retain their quiet
    diagnostic state. Classical normal and names resources require explicit
    opt-in, failed names resources fall through in registry order, and a
    trailing-star plan disables contingent as well as adaptive retries.
70. The native application loads optional `dict.cfg` plus its ordered resources
    from the XDG configuration directory before opening a document. Ctrl+D/F6
    opens one modeless lookup window seeded from the editor selection or caret
    word, routes options through the ordered dispatcher, and presents attributed
    results and nonquiet resource failures. Selected result rows insert as one
    paragraph-aware JWP transaction with portable undo; malformed reloads retain
    the prior working resource set and plain-text documents reject insertion.
71. Successful native EDICT searches also append to one separate modeless
    results window. Reports retain resource order and labels, cumulative
    rejected/nonquiet-failure status, visual-order multi-selection copy and
    insertion, explicit clearing, and callback-failure containment. Failed
    searches do not create an empty accumulated-results window.
72. A portable editable EDICT dictionary models optional raw-JWP headwords,
    required raw-JWP readings, and configured CP1250-CP1258 meanings. It parses
    mixed `user.dct` records with an optional internal-search LF, paired or final
    line endings, imported empty meanings, printable ASCII or JIS cells, and
    embedded meaning slashes; canonical output is LF-terminated without the
    internal prefix. New entries separately require a non-space reading and
    nonempty single-line meaning, while all paths enforce aggregate budgets.
73. Qt file I/O treats only a genuinely absent `user.dct` as an optional empty
    resource, validates configured code pages and limits before returning that
    absence, and parses every present file through the portable codec. Saving
    prepares the full canonical payload before `QSaveFile` replacement, so
    malformed input, unrepresentable meanings, dangling links, and unavailable
    output parents never publish partial state or damage an existing file.
74. A portable `user.dct` editor validates a complete candidate entry vector
    before publishing Add, Edit, Delete, Up, Down, or Sort. Sort reproduces the
    recovered reading-first, hiragana-before-katakana selection algorithm under
    an explicit comparison-work budget rather than trusting its stateful
    comparator to a standard sorting algorithm. Raw JWP reading and whole-row
    keys are precomputed once and preserve the configured meaning code page.
75. The modeless native `user.dct` dialog owns a working copy for Add, Edit,
    Delete, Move, Sort, and Import. Save and Insert cross explicit callback
    boundaries; failures, including non-standard exceptions, remain contained
    without publishing the working copy or closing the dialog. Interactive Add
    and Edit retain invalid fields in place and ask before accepting a non-kana
    reading, with No as the safe default; imported compatible entries remain
    governed by the broader portable format boundary.
76. MainWindow locates the configured user dictionary or synthesizes the
    recovered mixed, unindexed `user.dct` defaults. Configuration and updates
    are candidate-first; canonical bytes are atomically persisted before only
    the user search resource is replaced. Disabled entries remain unsearchable,
    failed saves preserve disk and live state, and display-row insertion is one
    portable-history JWP edit.
77. Strict UTF-7 support reproduces the recovered modified-Base64 direct-set
    behavior, literal-plus forms, and UTF-16BE wire representation while
    validating residual bits and surrogate structure. It is exposed through
    explicit text-file, native filter, and CLI selection; automatic detection
    deliberately retains JWPxp's UTF-8 default for the shared ASCII `.utf`
    payload space.
78. JFC plain-text compatibility tries strict UTF-8 before the recovered
    old-EUC repertoire: JIS X 0208 pairs, `0x8e` code-page bytes, and the shared
    77-entry `0x8f` JIS X 0212 mapping. Undefined recovered code-page bytes,
    unmapped pairs, and incomplete sequences fail rather than being replaced.
    Native extension routing, Open/Save As filters, the encoding menu, and
    `--encoding jfc` preserve this file policy; saves always produce UTF-8
    without a BOM. Core, atomic I/O, editor lifecycle, dialogs, and application
    tests cover the complete path.

The generic EUC-JP/Shift-JIS codecs reject JIS X 0201 halfwidth kana, JIS X 0212,
vendor extensions, malformed byte sequences, unassigned table cells, and
Unicode characters outside the recovered repertoire. Supporting any of these
requires a separate fixture-backed change rather than silent substitution.

## Native completion boundary

The recovered main-menu command inventory and configuration surface now have
tested native implementations or source-justified platform replacements and
exclusions. CMake/CPack delivery replaces the Windows registry installer, the
embedded handbook replaces WinHelp, and portable Qt MIME formats replace private
Windows clipboard IDs. Desktop targets share the same Qt Widgets application;
the WebAssembly target replaces local path ownership with explicit browser
upload/download and persistent IndexedDB storage, and excludes JPR workspaces
and printing where browser/Qt platform contracts cannot support them.

GitHub Actions build all four ports on demand and for `v*` release tags. A separate
workflow publishes the tested Web static site and matching corresponding source
to GitHub Pages from `master`. The command-inventory regression, Linux CTest
suites, hosted platform builds, and repository-owned browser smoke guard this
boundary; newly recovered behavior still requires an independent fixture-backed
change.
