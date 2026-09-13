# Advanced Install Dialog

JWPqt 2.04 has no Advanced Install dialog. The legacy dialog controlled Windows registry file associations, desktop shortcuts, and Start menu entries from within JWPce. That mechanism is obsolete in the native port because CMake/CPack and the target operating system own installation and desktop integration.

On Linux, package or private-prefix installation can place the supplied desktop entry, icon, and MIME XML in the usual XDG locations. Refresh desktop and MIME caches using the distribution's normal tools after installation. Put `jwpqt` on PATH when the desktop entry requires it. The MIME definitions identify JWP/JPR content, but do not forcibly set JWPqt as the default application. Change defaults only through the host desktop's standard association controls.

Windows ZIP and macOS DMG delivery use platform-native application icons and deployment support. The Web build cannot create desktop associations; it is served as a static site and uses explicit browser upload and download. No JWPqt operation creates legacy Start menu entries, registry classes, JFC shell verbs, or CE shortcuts.

For advanced runtime setup, use the explicit resource directory options instead of installer choices: `--config-dir`, `--user-data-dir`, and `--wnn-data-dir`. These settings make shared, test, and per-user configurations clear and diagnostic. Help > Runtime Resources or `--resource-report` shows what was actually loaded. See [Working with Projects](help:IDH_FILE_PROJECT) and [General Options](help:IDH_OPTIONS_GENERAL).
