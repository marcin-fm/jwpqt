# More About Installation

JWPqt deliberately separates application delivery from runtime resources. CMake and CPack create installable artifacts; the host desktop manages application launchers and file associations; the application resolves its own settings and optional lookup data at runtime. This is the native replacement for JWPce's executable-startup installation check, registry setup, and file-association dialog.

On Linux, a TGZ may be extracted into a private prefix, or a build may be installed with `cmake --install BUILD --prefix PREFIX` and optional DESTDIR staging. Ensure the executable is on PATH before registering the desktop entry and MIME data with normal distribution tools. Registration does not force JWPqt to become the default handler. On Windows and macOS, use the supplied native ZIP or DMG. On the Web, deploy the static-site root intact; browser security owns upload/download and persistent storage.

At runtime, default configuration and user-data locations come from Qt. `--config-dir`, `--user-data-dir`, and `--wnn-data-dir` select only JWPqt resources and avoid altering unrelated Qt desktop configuration such as themes. `--resource-report` prints the chosen paths and resource status. The embedded handbook and ordinary editing do not require optional dictionaries.

Do not manually recreate legacy registry keys or move CE help files. JWPqt neither reads nor writes the Windows registry for installation. Its offline handbook replaces WinHelp and permits only bundled pages and resources, not arbitrary local files or websites. See [Advanced Options](help:IDH_OPTIONS_ADVANCED) and [Support](help:IDH_SUPPORT_GENERAL).
