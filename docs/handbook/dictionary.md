# Dictionary Search

Dictionary lookup uses EDICT resources, independently of WNN conversion.
Results stay in the lookup window. Missing or invalid resources are reported;
the application does not download them automatically.

Opening lookup searches an actual first-paragraph selection when AutoSearch is
enabled. A new dialog can prefill the word under the cursor without automatically
searching that fallback. Disabling AutoSearch preserves an existing query draft.
The query has local Kanji/ASCII/JASCII input and shares Insert/Overwrite mode.

## Search Controls

Begin/End boundaries, Full ASCII and JASCII normalization affect matching.
Advanced enables bounded adaptive deinflection, not wildcard syntax. Its Always,
Show All and I-adjective options retain values while disabled. Contingent Search
retries eligible failed exact searches. Shift+Search forces one request without
changing the saved policy; ASCII, pattern and exact-hit gates still apply.

Options opens the Dictionary settings tab. All 21 source category exclusions
are available under Exclude Tagged Senses. They filter exact case-sensitive
metadata, not semantic content. Mixed recognized groups retain allowed senses;
unknown tags stop filtering that group. Advanced/name linkage acts only on user
toggles: enabling Advanced excludes personal/place names, while including either
category disables Advanced. Imported combinations are not silently normalized.

No Names controls both name categories; its mixed state preserves unequal
personal/place choices until clicked. Names performs one search including both
categories, without Advanced or Contingent retries, and leaves saved options
unchanged. These filters also apply to tagged names in ordinary dictionaries.

From Clipboard explicitly searches the first paragraph of clipboard text.
Monitor Clipboard is off by default and searches future external copies while
lookup is visible. Opening/enabling does not consume existing clipboard data.
Copies may enter persisted query history: enable monitoring only when wanted.
Queued searches coalesce and are cancelled by editing, selection changes,
disabling or closing; modal interactions and application-owned copies are
ignored. Visible lookup windows poll for external X11 ownership changes because
Qt may not expose their contents with the ownership signal alone. Unsupported,
malformed or oversized queries preserve the previous
query and results. Valid searches that fail retain their previous results and
show a nonmodal diagnostic. The limit is 100 characters, without truncation.

## Results

Compact layout, priority grouping and separators take effect on a successful
new search. Priority is determined from original records and search boundaries.
Labels are copyable display text but never insertable entries. Sort cycles
Reading, Length, Entry and Definition; Shift cycles backward, Ctrl reverses.
Sorting removes grouping labels and retains the completed compact/expanded
layout. It preserves canonical entries and the first duplicate's provenance.

Copy copies selected display text, including exact Unicode spacing. Insert or
Enter inserts whole canonical entries touched by the selection. Plain left
double-click inserts the clicked entry, not a previous selection. Printable
typing from results enters the local query; clipboard/navigation shortcuts stay
with results. Right-click a character for an independent information viewer.

History recalls without searching. User Dictionary opens the shared editable
working copy, which stays available after lookup closes. Add and Edit fields
have their own K/A/J mode buttons and use the document window's current
Insert/Overwrite mode. Pending romaji is completed before the entry is
validated. A reading must be nonempty and contain no spaces. Non-kana readings
remain supported for compatible data, but Add and Edit ask for confirmation and
use No as the safe default. Declining or correcting an invalid entry keeps all
fields in the editor.

Tools > User Conversions manages WNN readings and candidate lists in the same
way. Its Add and Edit fields also provide K/A/J input and shared overwrite
behavior. Readings must be nonempty hiragana and candidates must be nonempty.
Inflected entries must satisfy the recovered Godan, Ichidan or i-adjective
length, ending, cardinality and slash rules. An error keeps the same editor open
with the reading, candidates and inflection intact. Separate multiple candidates
with `/`. Imported records retain the broader legacy file compatibility rather
than being silently rewritten by editor validation.

Both editable lists support the recovered keyboard commands in addition to
their buttons: Insert adds an entry, Space edits the current entry, Delete
removes it, and Ctrl+Up/Down changes its order. Ctrl+C or Ctrl+Insert copies the
current displayed row without changing or saving the working copy. Tab, Return
and Escape keep their normal dialog behavior.

Ctrl+I opens an independent Character Information window for the first original
JIS character in the selected row. Ctrl+L or F5 opens Radical Lookup with that
character as its starting point, without moving the active document selection.
The row context menu includes Character Information, and F23 opens that complete
menu from the keyboard. User Conversions use the first reading character; the
EDICT editor uses the first headword character, or the reading when no headword
is present. Ctrl+F4 closes either editor through the same save prompt as its
title-bar close button.

Import in either user-dictionary window accepts multiple files, and local files
can be dropped directly on the window. Every file is parsed in source order
before the working copy changes, so a missing, malformed or oversized file does
not leave a partially imported dictionary. Saving remains a separate explicit
operation. User Conversions first warns that Import is additive and defaults to
No. Both Import commands start in the directory containing the configured user
file, rather than depending on the application's working directory.

Application exit checks both user-dictionary working copies before documents
or settings. Save publishes through the same atomic file boundary as the
window's Save button. Discard applies only to the current close attempt; if a
later document or settings prompt cancels the exit, the dictionary remains
modified and asks again. Cancel and failed saves keep the application open.

Closing a modified user-dictionary or user-conversion window from its title bar
asks Yes or No whether to save, with Yes as the safe default. Yes uses the same
atomic Save operation; a failed save keeps the window and its working copy open.
No closes without publishing the changes. The window's explicit Cancel button
is different: it deliberately closes without another question, matching the
original editor's discard command.

## Manage Dictionaries

Tools > Manage Dictionaries, or Dictionaries in lookup, opens a staged editor
for the configured binary `dict.cfg`. Add, remove and reorder entries, toggle
their search checkboxes, and edit names, paths, encoding, name role, classical
role, index, keep-loaded and quiet preferences. Removing an entry never deletes
its files. The single mixed/unindexed User entry cannot be removed; close its
editor before replacing resources so its working copy is not discarded.

Relative paths resolve from the registry's directory. Browse selects an exact
local file and runs the original bounded sample inference; Detect applies it to
the path already entered. You can also drop one or more local files on the
manager. Inference examines at most 2,048 bytes, proposes the first
slash-delimited description as the name, and enables a companion `.jdx` when
present. Detect asks before replacing an existing description and defaults to
No; declining retains the description while applying the other inferred fields.
Review the staged choice because structurally ambiguous byte sequences
can still require selecting EUC-JP, UTF-8 or mixed explicitly. Inspect validates
the selected resource and requested index, reporting its record count or error.
Mixed definitions use the configured default JWP code page captured at reload.
Buffered preferences round-trip but share the bounded native loader, not a
Win32 streaming implementation. Defaults stage CLASSICAL, EDICT and ENAMDICT
while retaining your user entry; no optional corpus is downloaded.

Save and Reload validates a complete candidate before atomically saving it.
Unavailable searched resources require the explicit allow checkbox; diagnostics
remain visible afterward. Resource-limit violations are fatal. An absent optional
user file need not exist until its first entry is saved. Existing lookup queries,
results and history remain unchanged; subsequent searches use the new order.
Cancel leaves both the live configuration and disk unchanged.

Old ANSI registries require an explicit original Windows code page. Conversion
to Unicode is staged and only saved on acceptance. Registry flags are written
canonically. If another process changes, deletes or creates the file while the
manager is open, saving fails rather than overwriting it. Cancel and reopen to
review that newer file. Application state, open documents, dictionary data and
indexes cannot be used as registry output targets.

[Runtime data](installation.md) | [Character information](kanji.md) | [Contents](start.md)
