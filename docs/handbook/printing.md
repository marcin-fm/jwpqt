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
in tenths of a point), not screen pixels. The current renderer is monochrome.

## Headers and Footers

JWP supports separate odd/even header and footer fields and first-page
suppression. The substitutions are case-insensitive:

| Code | Value |
| --- | --- |
| &A | Author |
| &C | Comment |
| &D | Date, yyyy/MM/dd |
| &F | Full file name |
| &K | Keywords |
| &L | Title |
| &N | Base file name |
| &S | Subject |
| &T | Time, HH:mm |
| &P | Page number |
| && | Literal ampersand |

Unknown codes remain literal. The job captures one timestamp. Headers must fit
the paper; invalid geometry fails before replacing an existing PDF.

Vertical follows the original physical-paper convention: Japanese glyphs are
counter-rotated, while Latin and specified punctuation stay horizontal, for
reading after turning the sheet clockwise. This is not a different top-to-bottom
document model. Exact old bitmap/vertical-font placement is not promised.

PDF text, geometry, pagination and cancellation are the project's printing
acceptance criteria. Custom legacy date/AM-PM formats, header-position tuning
and ASCII grid justification remain unapplied settings.

[Document formatting](files.md) | [Contents](start.md)
