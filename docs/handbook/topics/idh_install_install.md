# Installation

JWPqt 2.02 is packaged for Linux, Windows, macOS, and WebAssembly. Select the artifact for the target platform and architecture. Linux is distributed as a TGZ for a compatible system ABI; it is not an AppImage or universal binary and expects system Qt 6 Widgets, PrintSupport, Svg, standard C++ libraries, and a Japanese font such as Noto Sans CJK JP. Windows uses a ZIP with deployed Qt runtime files, and macOS uses a DMG application bundle. The Web artifact is a static site archive.

For a private Linux installation, extract the TGZ into a prefix and run `PREFIX/bin/jwpqt` from any working directory. A source build can use `cmake --install BUILD --prefix PREFIX`; package-managed or staged installs should follow the normal platform packaging procedure. The desktop entry, icons, and MIME definitions can be installed into the corresponding XDG locations, then desktop and MIME caches refreshed through distribution tools. File-type registration never forces JWPqt as the default application.

The Web archive must be served with its companion files unchanged: `index.html`, `jwpqt.js`, `jwpqt.data`, `jwpqt.wasm`, `qtloader.js`, and `qtlogo.svg`. Web Open uploads one file and Save downloads it; it does not install into a host filesystem.

This replaces the legacy self-installing executable and Windows registry associations. Installation is performed by CMake/CPack, native package delivery, or the host desktop, not by an in-application installer. See [Advanced Options](help:IDH_OPTIONS_ADVANCED) and [Deleting Files](help:IDH_FILE_DELETE).
