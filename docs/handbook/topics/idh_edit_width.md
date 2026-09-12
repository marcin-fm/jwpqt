# Text Display and Line Width

JWPqt displays native Japanese documents through a Qt rich document layout. The active line width policy is configured in Options and has three modes: **Dynamic**, **Fixed**, and **Printer**. Dynamic width follows the available editing area. Fixed uses the configured character width. Printer uses the document's printing-oriented line width. A fixed width defaults to 35 characters and is accepted only from 5 through 1000 characters.

Line width controls wrapping and therefore the visual lines used by Home, End, Ctrl+Shift+W, no-selection Copy/Cut, and Shift+Backspace/Delete. It does not rewrite paragraph text merely because the window is resized. A native paragraph retains its own formatting and may contain proportional spacing, indents, and structural page breaks. Text layout uses those values rather than treating every wrapped line as a saved newline.

The Options settings also retain margin-relaxation policies for punctuation and small kana; both are enabled by default. These policies help fitting Japanese text near a margin without inventing extra document characters. They are layout choices, not text conversion. For printable output, the native renderer uses the paragraph formatting and page layout; fixed Japanese cells and tab behavior remain relevant even where Qt supplies normal text layout.

The display font roles are independent of the desktop menu font. The Japanese editor, candidate bar, and lookup content default to 16 logical pixels. An independently configured ASCII font is matched to the Japanese role's height. Font selection affects presentation, printing, and clipboard images but does not change document text, JWP tokens, or Undo history.

For paragraph-specific indents and spacing, use **Format > Paragraph** as described in [Formatting Paragraphs](help:IDH_EDIT_PARAGRAPH). For actions defined by visual rather than stored lines, see [Using the Clipboard](help:IDH_EDIT_CLIPBOARD) and [Selecting Text](help:IDH_EDIT_SELECTING).
