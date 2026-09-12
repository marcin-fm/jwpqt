# How to Disable or Enable the Auto-Install Check

JWPqt has no auto-install check to disable or re-enable. JWPce performed a startup validation that could offer to install registry associations and shortcuts, then stored a choice not to show that dialog again. JWPqt intentionally does not implement this behavior because it does not use a Windows registry installer and must behave consistently on Linux, Windows, macOS, and WebAssembly.

If you need to change file associations, use the target operating system's normal application-default or MIME-association tools. Linux package integration can install the supplied desktop and MIME metadata, but it never forces a default application. A private-prefix extraction can be removed without changing separate user configuration or documents. Windows and macOS package delivery likewise leaves host application-default policy to the host system.

Do not seek a hidden preference, registry key, or command to restore the old installation prompt; none is source-supported. The closest native diagnostic is Help > Runtime Resources or `jwpqt --resource-report`, which reports configuration paths, loaded lookup resources, absent optional components, and failures. It is not an installer and does not change desktop integration.

This distinction matters for shared deployments: explicit `--config-dir`, `--user-data-dir`, and `--wnn-data-dir` choose JWPqt-owned resources without modifying global file associations or suppressing errors. See [Advanced Options](help:IDH_OPTIONS_ADVANCED) and [General Options](help:IDH_OPTIONS_GENERAL).
