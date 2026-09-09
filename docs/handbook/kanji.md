# Character Information and Kanji Lookup

Character Information opens an independent modeless window for the character
under the cursor or right-click target, without disturbing the source selection.
Right-clicking readings or other results can open further independent viewers.
Insert acts on the currently active document, not necessarily the original tab.

Character Info Setup configures 26 ordered fields, blank spacers, headings and
compact readings. More Info includes later fields and available references.
Missing metadata degrades to basic Unicode/codes with a diagnostic; unsupported
references are not invented. The legacy single-dialog setting is deliberately
unapplied because this application supports independent viewers.

## Lookup Tools

Native tools include graphical Radical, Bushu, Stroke/Bushu and Spahn selectors;
SKIP and Four Corner helpers; reading/meaning lookup; code and Index Lookup;
and the JIS character table. Installed data determines which indexes are
available. Clear and automatic search retain sensible pending-query boundaries.

Radical Lookup links the recovered equivalent radical variants. Opening it
seeds the current JIS kanji, or use From Clipboard to extract the first kanji's
radicals. Invalid or unavailable characters leave the current query intact.
The stroke-count arrows cycle through Any (zero), the selected-radical stroke
estimate, up to 30, then Any again. The estimate is only a stepping hint, not
an automatic filter or a guaranteed character count; counts can also be typed.
Choose Exact, +/- 1 or +/- 2, or edit the explicit minimum/maximum range.
Clear resets all of these controls and cancels pending search. With automatic
search disabled, changed criteria clear stale results until Search is pressed.
Options > Kanji Lookup saves Automatic Lookup and Rare Kanji Last, including in
projects. Auto is shared by the radical and code lookup windows in the current
workspace. Applying preferences preserves current results and selection and does
not start a search; Rare Kanji Last affects the next radical search. Reading and
Index remain explicit searches, retaining the last report until successful Search.
The Bushu stroke arrows also cycle Any, the selected radical's minimum, through
30, and back to Any; manual counts remain available.

Options also saves Nelson/classical Bushu matching, flexible kun readings,
partial-word meanings, SKIP miscodes, default index/reading types, equivalent
variant reduction and rare radical dimming. User changes in the lookup windows
are remembered; both Bushu pages remain synchronized. Variant reduction applies
to Stroke/Bushu and Spahn choices. Dimming uses the original fixed rare-choice
list in the Radical and Bushu grids and never prevents selecting those choices.

A saved index or reading type that is unavailable in the current metadata uses
a visible fallback without erasing the saved preference. SKIP miscodes remain
disabled when the data has no cross-references. Applying preferences does not
flush pending kana, search, or discard results. A new reading window initializes
its input mode for the selected type; an existing query keeps its local mode.

In radical, code and reading result strips, Enter inserts the selection without
searching, I or F23 opens information, and F4 closes the lookup. F2/period moves
forward and F3/comma moves backward; Ctrl moves five characters, including with
Left/Right. Standard selection/navigation keys remain available. C or Ctrl+C
copies the selection; Shift+C, Shift-click Copy or Shift+Space on Copy copies all
results in order without changing the selection. Successful manual Search focuses
its result strip. These shortcuts do not take over the Japanese query field.

Result strips provide canonical insertion and per-character information menus.
Count Kanji can inspect current or all open documents without changing their
text or accepting a conversion preview. Its snapshots follow edits and closed
tabs. The accumulated dictionary result window opens only on explicit request.
The Add/Remove kanji-color-list prompt provides K/A/J input, shared
Insert/Overwrite behavior and pending-kana completion before validating the
edited list.

Dark and light artwork follows the native palette. Japanese content fonts can
be configured independently from desktop menu fonts in Options.

[Data setup](installation.md) | [Settings](settings.md) | [Contents](start.md)
