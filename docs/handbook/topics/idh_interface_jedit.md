# Japanese Edit Controls

JWPqt provides Japanese-aware editor and query fields rather than the legacy custom Windows control class. The main editor supports Japanese document input, selection, search, clipboard operations, character information, and kana-to-kanji conversion. Dictionary and Reading Lookup queries are Japanese-aware single-line fields with their own Kanji/ASCII/JASCII mode control; their local choice does not change the main document's mode.

In Kanji mode, printable desktop romaji is composed to hiragana or katakana according to the recovered composer. ASCII inserts directly. JASCII produces recovered full-width characters. F4 toggles Kanji and ASCII in the relevant field; document shortcuts include Ctrl+K for Kanji, Ctrl+Alt+A for ASCII, and Ctrl+J for JASCII. Query fields share the workspace Insert/Overwrite state while retaining their own input modes. Invalid input, validators, limits, and read-only targets are checked before text is erased.

Query history is available for dictionary, Find, and Replace fields. Up recalls older entries; Down moves toward newer entries and a blank draft, or can open the history list after an edited draft. Recall resolves pending kana once but does not start a search or replace existing results. History persistence and capacity are configured under Options > History.

The editor is not limited to a one-line legacy control. It can be a Japanese JWP document or a plain-text document with Japanese input. See [Input Modes](help:IDH_TEXT_INPUTMODES), [Searching and Results](help:IDH_DICT_RESULTS), and [Search and Replace](help:IDH_EDIT_SEARCH).
