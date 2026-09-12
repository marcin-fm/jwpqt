# JWPqt 2.00

Release date: 2026-09-12

JWPqt 2.00 is the first complete native Qt 6 continuation of JWPxp 1.67. It
preserves JWP/JCE compatibility boundaries while replacing the Win32 application
layer with portable C++ and native Qt implementations for Linux, Windows, macOS,
and WebAssembly.

## Highlights

- Native multi-document editing, Japanese kana input, WNN conversion, undo,
  search/replace, formatting, projects, sessions, recent files, and atomic saves.
- JWP/JCE, JFC, UTF-8, UTF-16, UTF-7, EUC-JP, Shift-JIS, New/Old/NEC JIS, and
  legacy code-page compatibility with loss-aware conversion.
- Japanese-English dictionary search, user dictionaries, query history,
  deinflection, patterns, sorting, filtering, and canonical result insertion.
- Character Information and the complete kanji lookup family, including unified
  Radical, Stroke Count, Bushu, Stroke/Bushu, SKIP, Spahn-Hadamitzky, Four Corner,
  Index, Reading, and code-table workflows.
- Native PDF printing and preview, horizontal and legacy vertical layout,
  headers/footers, page ranges, raster fonts, colors, and selection output.
- Complete searchable 125-topic offline JWPce handbook, responsive vector artwork, configurable themes,
  toolbars, fonts, colors, layout, history, and source-compatible preferences.
- GitHub Actions packages every port on demand and for release tags. The Web port
  is also built, smoke-tested, and published through GitHub Pages.

## Packages

- Linux: `jwpqt-2.00-Linux-<architecture>-noncommercial-data.tar.gz`
- Windows: `jwpqt-2.00-Windows-<architecture>-noncommercial-data.zip`
- macOS: `jwpqt-2.00-Darwin-<architecture>-noncommercial-data.dmg`
- WebAssembly: `JWPqt-2.00-WebAssembly-Noncommercial-Data.tar.gz`
- Corresponding source: `jwpqt-2.00-Source-Noncommercial-Data.tar.gz`

The normal packages include the original JWPce/JWPxp EDICT, ENAMDICT,
KANJIDIC-derived, radical, stroke, and related lookup payloads as separate,
unmodified files. Those datasets are not GPL program code. They retain the
noncommercial and permission conditions in the bundled original notices and
must not be sold or relicensed as GPL. Configure with
`-DJWPQT_BUNDLE_LEGACY_DATA=OFF` for a data-free GPL-only package.

## Compatibility And Upgrading

- JWPqt uses the platform's standard application configuration and data paths.
  Existing native settings, query histories, learned conversions, user
  dictionaries, recent files, and session manifests remain compatible.
- JWP/JCE documents and JPR projects retain their legacy compatibility formats.
  Damaged JWP recovery is interactive and never weakens strict automated,
  project, or Revert paths.
- Explicit resource directories and user `dict.cfg` files override packaged
  lookup data. Partial or corrupt explicit overrides fail rather than silently
  mixing unrelated resources.
- The complete recovered JWPce/JWPxp source lineage is preserved by the release
  tags through `jwpxp-1.67`; native 2.00 development follows that history.

## Platform Notes

- Linux packages use the system Qt 6 runtime and a compatible C++ ABI.
- Windows ZIP and macOS DMG packages include deployed Qt runtime components.
- WebAssembly uses browser upload/download and IndexedDB persistence. Browser
  security prevents JPR referenced-file workspaces, and Qt WebAssembly has no
  PrintSupport, so project and printing commands are visibly unavailable there.
- Physical printer hardware is not an acceptance gate. PDF text, pagination,
  rendering, ranges, cancellation, and atomic replacement are tested directly.

## Verification

The release gate includes complete GCC and Clang ASAN/UBSAN suites, data-free
configuration, package extraction and installed-runtime checks, exact Qt 6.8.3
and Emscripten 3.1.56 WebAssembly build/smoke tests, workflow validation, and
license/data inventory checks. Windows and macOS packages are built on their
native GitHub-hosted runners.
