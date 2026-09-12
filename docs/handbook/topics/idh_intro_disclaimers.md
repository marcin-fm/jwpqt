# Disclaimers

JWPqt is distributed without warranty, including implied warranties of merchantability or fitness for a particular purpose. The GPL contains the governing warranty language for the program; third-party datasets and fonts may carry additional or different notices. Review the installed GPL and original notices before relying on the software or redistributing a package.

Compatibility is deliberate but bounded. JWPqt preserves supported JWP/JCE document structures, text encodings, Japanese editing, portable undo, and many recovered commands. It does not claim exact Win32 pixel placement or identical raster rounding on every desktop. Native PDF generation, text extraction, pagination, ranges, cancellation, and atomic output replacement are the printing verification boundary; physical-printer hardware is not an acceptance requirement and cannot be guaranteed across every platform backend.

Resource loading is explicit. A valid packaged fallback may be replaced by an explicit configuration directory, but incomplete or malformed overrides fail visibly rather than being silently combined with unrelated data. `--resource-report` and Help > Runtime Resources show loaded sources, skipped records, and failures. A successful report command does not mean every optional dictionary is available.

Protect personal material when requesting support. Resource diagnostics, document paths, query history, user dictionaries, and document text can contain private information. The application makes atomic saves where supported, but users remain responsible for backups and verifying exported data. See [Saving Files](help:IDH_FILE_SAVE) and [Fonts Introduction](help:IDH_FONTS_INTRO).
