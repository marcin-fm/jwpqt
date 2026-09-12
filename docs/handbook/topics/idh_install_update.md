# Updating JWPce

Update JWPqt by installing the new platform package or building and installing the newer source release into the intended prefix. There is no `UPDATE.EXE`, no built-in binary patch step, and no registry migration utility. Those were Windows-era installer mechanics and are intentionally excluded from the native port. Package managers, archive replacement, or CMake installation are the supported delivery paths.

Before updating, keep backups of documents and any user-managed configuration or data. JWPqt uses platform-standard application locations by default. On Linux these are normally `~/.config/jwpqt/jwpqt` for configuration and `~/.local/share/jwpqt/jwpqt` for user data; Windows and macOS use their Qt standard application locations. Use `jwpqt --resource-report` to identify actual resolved paths. Explicit `--config-dir` and `--user-data-dir` options are preferable to changing global environment settings merely to isolate the application.

Existing native settings, query history, learned conversions, user dictionaries, recent files, sessions, JWP/JCE documents, and JPR projects are supported compatibility surfaces. Nonetheless, inspect Runtime Resources after an update, particularly when supplying your own dictionaries. An explicit configuration resource or `dict.cfg` takes precedence over packaged fallback. Incomplete external WNN data requires both `wnn.dat` and `wnn.dix`; it is an error rather than an invitation to combine old and new files.

Do not overwrite a known-good custom registry or user dictionary while testing replacement resources. See [Advanced Options](help:IDH_OPTIONS_ADVANCED) and [What is saved with a JWPce File?](help:IDH_FILE_CONTENTS).
