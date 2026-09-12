# User Kana to Kanji Conversions

JWPqt can supplement WNN conversion candidates with user conversions. The editable user-conversion file is `user.cnv` in the selected user-data directory. It is distinct from the separate storage for learned choices, so changing a defined user conversion does not erase learned candidate preferences. The user-conversion manager is available through **Tools > User Conversions**.

Use the manager to maintain readings and candidate lists that should be offered during kana-to-kanji conversion. A reading is Japanese input for the conversion system, and candidates must be representable JWP text. The dialog validates entries rather than creating an unusable mapping. A reading and its candidate list cannot be empty; inflected entries have additional constraints, including their required final kana and candidate count. Invalid entries are reported and are not silently accepted.

During normal Kanji-mode input or selected kana conversion, matching user entries appear beside the WNN candidates. The candidate strip remains a preview: click a candidate or cycle with `Space`, `Shift+Space`, or Convert, then accept with `Enter` or `Escape`. The accepted replacement remains one undoable conversion transaction. This means a user candidate can be tried and reverted without rebuilding the original reading manually.

Import is additive, not a replacement. JWPqt warns before importing user conversions and defaults the warning answer to No. Save and import operations affect the user data, not the open document until a candidate is accepted. The conversion tools remain available only when their required resources and active target are valid; a read-only document or active conflicting preview prevents destructive insertion.

Use explicit conversion for selected text and inline conversion for active input. See [Explicit Kanji Conversion](help:IDH_TEXT_EXPLICITKANJI), [Inline Kanji Conversion](help:IDH_TEXT_INLINEKANJI), and [Undo and Redo](help:IDH_EDIT_UNDO).
