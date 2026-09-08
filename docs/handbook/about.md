# About jwpqt

jwpqt is a native Qt 6 Linux port of JWPxp 1.67, descended from JWPce and JWP.
It is still under development and is not a claim of complete JWPxp parity.
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
with your distribution's Qt packages. Optional WNN, EDICT, KANJIDIC-derived,
SKIP, frequency and radical/stroke data have their own notices and conditions.
Their inclusion in an old archive does not make them GPL program data. They are
not included in the native package and are never downloaded automatically.

## Support and Diagnostics

Help > Runtime Resources and `--resource-report` provide local diagnostics.
Do not publish private document text, history files, user dictionaries or paths
without reviewing them first. Known open porting areas include remaining
dictionary/lookup policies, toolbar and font roles, auxiliary result-list Find
and some legacy print tuning. The bundled handbook distinguishes implemented
native behavior from these gaps.

[Contents](start.md) | [Installation](installation.md)
