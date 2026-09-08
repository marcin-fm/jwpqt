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
working copy, which stays available after lookup closes. Registry management,
clipboard watching and combined/one-shot Names controls are still incomplete.

[Runtime data](installation.md) | [Character information](kanji.md) | [Contents](start.md)
