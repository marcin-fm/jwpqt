# What if I already have JWP?

Keep an existing JWP or JWPce installation separate from JWPqt. JWPqt is a native Qt 6 application, not an in-place upgrade of its Win32 predecessor, and it does not use the old program's registry setup. Its native configuration and data paths are separate. The safest migration is to copy documents and make backups, then open a copy in JWPqt and verify content, encoding, layout, and print output before replacing any established workflow.

JWPqt supports JWP and JCE structured documents. New Japanese documents default to `.jce`; saving an existing `.jwp` keeps that extension unless a different format is selected. The two extensions use the supported structured JWP codec. Save As offers both structured formats and the supported text encodings. Export Copy writes another format without changing the current document path, saved baseline, modified state, or undo history, which makes it suitable for migration tests.

Do not assume old settings, query histories, user conversion caches, or dictionary registries can be copied blindly. Native settings use Qt application locations and explicit configuration options. A user `dict.cfg` overrides packaged dictionary fallback completely; malformed or partial resources report errors rather than mixing with installed data. The old `+directory` and `-directory` startup forms are not accepted; use `--config-dir`, `--user-data-dir`, and, if needed, `--wnn-data-dir`.

Legacy file associations remain owned by the operating system. Choose defaults through the host desktop only if wanted. See [File Types](help:IDH_FILE_TYPES) and [Working with Projects](help:IDH_FILE_PROJECT).
