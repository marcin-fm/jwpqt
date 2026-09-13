# Windows CE

Windows CE and PocketPC are not supported JWPqt 2.04 targets. JWPce's CE build existed for constrained handheld hardware and therefore had dedicated processor binaries, small-screen dialogs, Pocket Internet Explorer help, limited clipboard behavior, and unavailable printing or network-startup features. None of that platform layer is emulated by the native Qt port.

The supported targets are Linux, Windows, macOS, and WebAssembly. Desktop ports share the Qt Widgets application. The WebAssembly port is the source-justified modern portable/browser counterpart for limited local ownership: it retains the Qt interface, embeds Japanese text support, uploads a local file through the browser, downloads saved output, and stores preferences and optional named-session references in IndexedDB.

The Web port is not a Windows CE compatibility environment. It has no stable local path workspace, so JPR referenced-file projects are visibly unavailable. Qt WebAssembly has no PrintSupport, so printing and print preview are unavailable. These exclusions follow browser and Qt platform contracts rather than being silently degraded. Desktop builds retain projects and native PDF printing.

Do not copy old CE executables, processor-specific files, CE configuration, Pocket Help content, or CE registry/shortcut instructions into a JWPqt install. Use the package for the actual target platform and the embedded offline handbook. See [Opening an Existing File](help:IDH_FILE_OPEN) and [Display Options](help:IDH_OPTIONS_DISPLAY).
