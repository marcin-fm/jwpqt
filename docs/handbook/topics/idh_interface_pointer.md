# Pointers and Alt+tap

JWPqt uses the platform's normal pointer conventions. A mouse, trackpad, touchscreen, or other Qt-supported pointing device selects text, activates controls, and opens context menus according to the host desktop. The legacy Windows CE distinction between mouse clicks, pen taps, Action buttons, and Alt+tap exists only as historical context; Windows CE is not a supported target.

In the editor, Shift+left-click opens Character Information for the exact character. Alt+left-click opens the native editor popup menu. Holding an unmodified left click without dragging opens the same popup after the platform double-click delay. The Menu key and Shift+F10 open the editor popup at the caret. Double-click and Ctrl+left-click use the recovered source word boundaries for selection.

Right-click behavior is also available in supported list and result controls. For keyboard access, F23 opens a native row context menu in relevant editors and lookup results. Context menus offer the operations appropriate to their owner, such as copying, character information, lookup, insertion, or list editing. They do not emulate obsolete CE shell behavior.

Desktop input-method conversion is owned by the host system. A historical Ctrl+M Windows IMM command is not reproduced on Linux; native documents discard pending romaji and show a one-time notice because IME state belongs to the desktop. For JWPqt's own romaji composer, use Kanji mode and the verified shortcuts in [Input Modes](help:IDH_TEXT_INPUTMODES). See also [Searching and Results](help:IDH_DICT_RESULTS).
