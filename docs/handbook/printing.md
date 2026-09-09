# Printing and PDF

Print and Print Preview freeze document text, metadata, font and selection
before opening dialogs. Later editor changes do not alter that job. Preview
shows the complete snapshot in natural page order and can print the same
snapshot through the protected Print dialog.

Output supports exact selection, page ranges, reverse order and collated or
uncollated copies. PDF files are staged and atomically published only after a
successful, uncancelled job. Open documents and application files are protected
destinations. Already spooled physical pages cannot be recalled.

Printer Setup initializes physical margins/orientation and applies them to the
original document as one undoable change. Print font size is in points (stored
in tenths of a point), not screen pixels. Options > Printing can enable kanji
colors when a list-color mode is active. The job captures the list and policy;
later changes do not change its body or header colors. Transient editor
highlights and arbitrary rich-text colors are not copied into print output.

## Headers and Footers

JWP supports separate odd/even header and footer fields and first-page
suppression. The substitutions are case-insensitive:

| Code | Value |
| --- | --- |
| &A | Author |
| &C | Comment |
| &D | Configured date pattern |
| &F | Full file name |
| &K | Keywords |
| &L | Title |
| &N | Base file name |
| &S | Subject |
| &T | Configured time pattern |
| &P | Page number |
| && | Literal ampersand |

Unknown codes remain literal. The job captures one timestamp. Headers must fit
the paper; invalid geometry fails before replacing an existing PDF.

Options > Printing sets date/time patterns and AM/PM text. Within these patterns,
`&Y` is the full year, `&y` a two-digit year, `&M`/`&m` month, `&D`/`&d` day,
`&H` 24-hour time, `&h` 12-hour time, `&N`/`&n` two-digit minutes and `&A`/`&a`
the literal AM/PM text. `&&` is an ampersand. Defaults are `&y/&M/&D` and
`&h:&N &A`. The recovered legacy rules use AM at noon and hour zero at midnight.
Date/time patterns hold 19 JWP characters; AM/PM text holds 9. Unused legacy
array cells remain preserved. Text unavailable in the current code page is
shown as a read-only placeholder rather than silently rewritten.

Left/right header extensions are measured in characters; header/footer distances
are measured in lines, in hundredths, from 0 to 10. Preview uses these same
settings. Excessive offsets that place text outside the paper fail safely.

Vertical follows the original physical-paper convention: Japanese glyphs are
counter-rotated, while Latin and specified punctuation stay horizontal, for
reading after turning the sheet clockwise. This is not a different top-to-bottom
document model. Exact old bitmap/vertical-font placement is not promised.

PDF text, geometry, pagination and cancellation are the project's printing
acceptance criteria. ASCII grid justification and exact original raster vertical
substitutions remain separate fidelity work.

[Document formatting](files.md) | [Contents](start.md)
