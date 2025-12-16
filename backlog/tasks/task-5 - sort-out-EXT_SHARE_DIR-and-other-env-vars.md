---
id: task-5
title: sort out EXT_SHARE_DIR and other env vars
status: To Do
assignee: []
created_date: "2025-12-16 10:38"
labels: []
dependencies: []
---

It'd be simpler to just move to:

- EXTEMPORE_PATH (same as EXT_SHARE_DIR, with the latter printing a deprecation
  warning but still working)
- EXTEMPORE_ARGS (a string of default args... as if they'd been passed to the
  command line)

There are a few other EXT\_\* vars (most for the build process, but some for
runtime as well I think...) and we should do a thorough audit to see if they're
still needed or can be removed.
