# Japanese Input, Find and Replace

Kanji mode composes romaji into kana. ASCII mode inserts literal text; JASCII
uses recovered full-width mappings. Insert toggles overwrite without accepting
a conversion. Clipboard and lookup insertion remain insert operations. Native
and unrestricted Unicode documents both support undo and lookup insertion.

## Conversion

Options > Display and Files also provides Old Katakana Vowel Handling (off by
default). Normally a quote after a pending uppercase A/I/U/E/O emits the katakana
vowel and consumes the quote. Old behavior also emits the original quote mapping:
the closing corner bracket for apostrophe, or dakuten for double quote. The setting
applies to document input, Japanese query fields and selected-romaji replay.
Changing it preserves pending input and takes effect on the next key; each
workspace retains its own policy. Settings and projects preserve the choice.

## Line Copy And Cut

Ctrl+C and Ctrl+X use the current selection normally. If there is no selection,
Ctrl+C copies the current wrapped visual line and restores the exact cursor;
Ctrl+X cuts that visual line as one undoable edit. Native JWP clipboard data keeps
its original byte/JIS identity. An empty line copies or cuts nothing, and Cut is
disabled in a read-only document.

## Word And Line Selection

Ctrl+W selects a source-style word using separate ASCII, punctuation, JASCII,
kana and kanji classes. Whitespace advances to the following word, a caret at
the paragraph end selects the preceding word, and the original kana long-vowel
and repetition-mark rules are retained. Ctrl+Shift+W selects the current
wrapped visual line. Both commands finish pending Japanese input first and do
not edit the document. Convert Selection uses F2 or Ctrl+>.

The mouse follows the same boundaries: double-click or Ctrl+left-click selects
a source-style word. Shift+left-click opens Character Information for the
exact character under the pointer; Alt+left-click opens the editor popup.
Holding an unmodified left click without dragging also opens the popup after
the platform double-click delay. Release or drag cancels the hold. Pending kana
is committed at its original caret before a mouse press moves it.

WNN conversion requires wnn.dix and wnn.dat. Choose a candidate in the candidate
bar and accept it as one undo transaction. User conversions and learned choices
have separate user-data files. Selected printable romaji can be converted to
selected kana; a second Convert can invoke WNN. Invalid or incomplete replay
does not delete the selection. Lowercase unselected words are not scanned
backward implicitly. Accept an active conversion before operations that would
invalidate its source.

Options > Display and Files controls whether an explicit Convert returns native
Japanese documents to Kanji input mode (enabled by default). Disable it to retain
ASCII or JASCII. Changing this preference does not accept an active preview;
the next explicit Convert changes the mode and continues the conversion. Automatic
conversion and unrestricted Unicode editing keep their existing behavior. This is
an input-mode policy, not a document edit, and persists in settings and projects.

The optional Ctrl+Up/Down conversion policy is off by default. In a native
Japanese document with selected text or a conversion range, Ctrl+Up converts or
cycles forward and Ctrl+Down cycles backward. The initial conversion uses the
normal first candidate in either direction. Shift does not reverse these rules.
Candidate cycling preserves the live preview and its single undo transaction.
Otherwise Ctrl+Up/Down scrolls the document viewport by one display line without
changing an existing selection. This replaces Qt's unrelated block-navigation
shortcut and applies to native and unrestricted Unicode documents. Query and
result-list shortcuts are independent. The conversion setting is retained by
settings and projects.

## Find and Replace

Options > History sets the native Japanese undo depth (3–1000, default 50).
Reducing it keeps the nearest undo and redo entries and permanently removes older
ones without changing the current document or saved/dirty state. Increasing it
does not recover discarded history. Finish an active conversion before changing
the limit. New documents, settings and projects retain the selected depth.
Unrestricted Unicode editors retain Qt's history; the native depth setting never
truncates their text or silently clears their undo stack.

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
