# Network or Multi-configuration Installation

JWPqt supports shared or multiple configurations through explicit directories, not legacy command-line shorthand. Use `--config-dir /path/to/config` for application settings and dictionary registry data, `--user-data-dir /path/to/user-data` for WNN learning and user conversions, and `--wnn-data-dir /path/to/wnn` when WNN data should come from a distinct location. These options replace JWPxp's first-argument `+directory` and `-directory` forms.

The replacement is intentional. The old syntax implicitly split multiple Win32 configuration paths, and its minus form could suppress resource-write errors. Native options name their purpose directly, work across desktop ports, and keep failures visible. Do not use a deliberately unwritable directory to silence diagnostics: Runtime Resources and normal file errors should remain actionable.

Configuration includes `jwpqt.cfg`, dictionary configuration such as `dict.cfg`, and related preferences. User data includes WNN `user.sel` and `user.cnv`. An editable EDICT `user.dct` normally remains beside `dict.cfg` unless the registry names an absolute path. Separate per-user directories prevent users from overwriting shared learned conversions or settings, while a managed configuration directory can provide common lookup resources.

When `dict.cfg` is absent, normal builds discover packaged data. If a configuration directory supplies `dict.cfg` or explicit kanji resources, that explicit set overrides the fallback; it must be complete and valid. Use `--resource-report` after provisioning. The Web port persists its profile in browser IndexedDB, not a shared network filesystem. See [Fonts Introduction](help:IDH_FONTS_INTRO) and [Advanced Options](help:IDH_OPTIONS_ADVANCED).
