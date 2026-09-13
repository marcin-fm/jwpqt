# About jwpqt

jwpqt is a native Qt 6 port of JWPxp 1.67 for Linux, Windows, macOS, and
WebAssembly, descended from JWPce and JWP.
Its recovered user-visible command and configuration surfaces have been audited;
each command has a tested native implementation or a documented platform
replacement or exclusion.
The running version is displayed in this window's status and by `jwpqt --version`.

## Credits and License

Original JWPce program and documentation: Glenn Rosenthal, 1997-2005.
JWP origin and interface: Stephen Chung. JWPxp modifications are credited in
the preserved source history and release notes. Native port contributions retain
their source history; current port commits are authored by marcin-fm.

Native source files declare GPL-2.0-or-later. This program comes without warranty,
including merchantability or fitness for a particular purpose. Read the full
[GNU General Public License](gnugpl.txt) and the preserved
[original copyright and data notices](_cpright.txt), including restrictions
specific to third-party components. Those original notices are not replaced by
this summary.

Qt is dynamically linked and separately licensed; consult the license supplied
with your distribution's Qt packages. WNN, EDICT, KANJIDIC-derived, SKIP,
frequency and radical/stroke data have their own notices and conditions. Normal
2.01 packages carry the original lookup payloads as separate, unmodified files
under those terms; their inclusion does not make them GPL program data.
Data-free builds omit the restricted payloads, and nothing is downloaded
automatically.

## Support and Diagnostics

Help > Runtime Resources and `--resource-report` provide local diagnostics.
Do not publish private document text, history files, user dictionaries or paths
without reviewing them first. Windows Install is replaced by CMake/CPack and
native desktop/Web delivery, WinHelp by this embedded handbook, and private
Windows clipboard formats by portable Qt MIME formats. The Qt screen line breaker
can group adjacent closing marks before per-character formatting; the portable
planner and PDF renderer still permit only the first mark to hang, while the
screen preserves exact document text rather than inserting hidden characters.
PDF rendering is the accepted printing gate; physical printer hardware was not
tested.

[Contents](start.md) | [Installation](installation.md)
