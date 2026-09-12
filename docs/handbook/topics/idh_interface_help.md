# Help

Help > Contents opens JWPqt's embedded offline handbook. It is packaged with the application and does not require a network connection or optional dictionary data. The same handbook can be opened at startup with `jwpqt --handbook`. The application reuses one help window without changing document text, selection, or undo history.

Press F1 in the editor or an eligible dialog to open a relevant handbook topic. The routing recognizes editor, Find/Replace, dictionary, kanji/JIS, printing/page-layout, settings/toolbar, project, and file contexts; otherwise it opens Getting Started. Help is context-oriented, but it does not claim a separate page for every individual legacy control.

The window has Back, Forward, Contents, Zoom Out, and Zoom In controls. Its search field searches bundled topic titles and handbook text. It does not search private documents, dictionary data, history, or arbitrary files. Search results state the number of matching handbook topics; press Enter to open the first match.

For safety and portability, help opens only whitelisted embedded pages and resources. It never loads arbitrary local files, websites, or external links. This is the native replacement for WinHelp and the Windows CE Pocket Internet Explorer help system. The legacy topic-navigation limitations of CE therefore do not apply, because CE is unsupported.

Use Help > Runtime Resources for local data diagnostics, and About for GPL, credits, and original notices. See [Support](help:IDH_SUPPORT_GENERAL) and [Advanced Options](help:IDH_OPTIONS_ADVANCED).
