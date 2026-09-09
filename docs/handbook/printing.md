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
The margin-relaxation settings also apply to print: one eligible closing
punctuation or small-kana cell may extend into the physical right margin.

## Headers and Footers

Options > Default Page sets the margins and orientation for fresh Japanese
documents and newly imported text. Existing JWP files retain their stored layout.
Page Layout > From Default copies the defaults into the current document;
Set as Default stages the current margins and orientation as the next defaults.
Cancel discards both staged changes. Accepting a page change is undoable in the
document; undo does not revert application preferences. Headers, summary and
body text are not copied into the defaults. Text exports still require approval
when page layout cannot be retained by their file format.

Options > Default Page can display measurements in inches or centimeters. The
same unit is used by Page Layout. Changing the display unit converts the values
shown in those dialogs; JWP documents and saved defaults continue to store
margins in inches, so switching units without editing a margin does not round or
rewrite it.

JWP supports separate odd/even header and footer fields and first-page
suppression. The substitutions are case-insensitive:

Header, footer, title, subject, author, keyword and comment fields provide the
same K/A/J input control and Insert/Overwrite policy as document editing.
Pending kana is completed before Page Layout validates and applies the metadata.

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
These four pattern fields also provide K/A/J input and the shared
Insert/Overwrite policy; they start in ASCII mode because the source defaults
are substitution patterns.

Left/right header extensions are measured in characters; header/footer distances
are measured in lines, in hundredths, from 0 to 10. Preview uses these same
settings. Excessive offsets that place text outside the paper fail safely.

The ASCII grid control defaults on. For native JWP documents it distributes
next-cell padding before tabs and on wrapped lines. Final paragraph ends and
ASCII directly followed by JIS text are not padded. Japanese characters occupy
fixed cells and tabs advance one cell, even with padding off. Grid-aware wrapping
preserves text; unrestricted Unicode documents retain ordinary Qt layout.

Vertical follows the original physical-paper convention, for reading after turning
the sheet clockwise. TrueType vertical alternates rotate Japanese punctuation as
well as letters; raster fonts use their source exception and ink-position rules.
Latin stays upright. This is not a different top-to-bottom document model or a
promise of pixel-identical Win32 rounding.

PDF text, geometry, pagination and cancellation are the project's printing
acceptance criteria; physical-printer testing is waived. Captured fonts, colors,
header patterns and grid preferences do not change with later Options edits.

[Document formatting](files.md) | [Contents](start.md)
