# Removing JWPce

Remove JWPqt through the mechanism that installed it. For a private Linux archive extraction, remove that installation prefix. For a system package, use the package manager or the installation manifest procedure. For a CMake installation, remove only files known to belong to that chosen prefix. On Windows and macOS, remove the delivered application according to normal platform practice. A deployed Web site is removed from its hosting location, not from a browser as an installed desktop program.

Do not delete user configuration or documents merely because the application is removed. JWPqt keeps them separate from the installation. Default configuration and user data use Qt application locations; on Linux they are normally `~/.config/jwpqt/jwpqt` and `~/.local/share/jwpqt/jwpqt`. These can contain settings, query history, learned conversions, user conversion data, user dictionaries, recent-file records, and sessions. Back up anything wanted, then remove it separately and deliberately.

The program never installs registry associations, so there are no JWPqt registry keys to edit during removal. Likewise, there is no `UPDATE.EXE`, WinHelp cache, or CE Start-menu shortcut cleanup procedure. If desktop metadata was installed manually, remove only the corresponding desktop, icon, and MIME entries using the normal desktop/package process.

Use `jwpqt --resource-report` before cleanup if configuration or data directories are uncertain. It identifies actual resolved locations without modifying them. See [Advanced Options](help:IDH_OPTIONS_ADVANCED) and [Backup Files](help:IDH_FILE_BACKUP).
