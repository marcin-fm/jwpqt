# Japanese Input, Find and Replace

Kanji mode composes romaji into kana. ASCII mode inserts literal text; JASCII
uses recovered full-width mappings. Insert toggles overwrite without accepting
a conversion. Clipboard and lookup insertion remain insert operations. Native
and unrestricted Unicode documents both support undo and lookup insertion.

## Conversion

WNN conversion requires wnn.dix and wnn.dat. Choose a candidate in the candidate
bar and accept it as one undo transaction. User conversions and learned choices
have separate user-data files. Selected printable romaji can be converted to
selected kana; a second Convert can invoke WNN. Invalid or incomplete replay
does not delete the selection. Lowercase unselected words are not scanned
backward implicitly. Accept an active conversion before operations that would
invalidate its source.

## Find and Replace

Edit > Find and Replace open modeless dialogs with their own Japanese input
fields. Search and replacement histories are separate, persistent lists. Up,
Down and the History buttons recall without searching; list deletion is
immediate even if the chooser is subsequently cancelled.

Ignore ASCII Case and Width Insensitive normalize only the supported Latin
letters and digits, not arbitrary Unicode or punctuation. Searches are literal,
within paragraphs, not regular expressions. Direction, Wrap Around, All Files
and Keep Dialogs Open govern traversal. All Files walks tabs circularly and
disables the separate wrap control. Find Next/Previous reuse the last query.

Replace Next asks about a match; Review provides Yes, No, Yes to All and Cancel.
Replace All validates the original non-overlapping match snapshot. It never
searches newly inserted replacement text. An empty replacement deletes text.
Each accepted occurrence is independently undoable; cancelling a multi-file
review retains completed edits and unprocessed text, not an all-files rollback.

Read-only documents, invalid encodings, changed selections or documents during
confirmation stop unsafe replacement. Content and work limits fail explicitly
instead of truncating a large search silently.

## Find In Results

Focus dictionary results, accumulated results, kanji lookup/count results or a
user-dictionary list and press Ctrl+F. The context menu offers Find, Next and
Previous as well. Japanese input, search history, case and width preferences,
direction, wrap and keep-open behavior are shared with document search, but
replacement and all-files controls are not available here.

Find advances past the current logical entry and selects the whole next match.
It does not match across unrelated entries or turn grouping labels into records.
If only the current entry matches, it reports no other match. F3/Shift+F3 repeat
in ordinary lists; kanji result strips retain F3 navigation, with repeated Find
available in the context menu. Finding leaves documents and canonical insertion
unchanged, including after results are sorted or refreshed.

[Documents](files.md) | [Settings and histories](settings.md) | [Contents](start.md)
