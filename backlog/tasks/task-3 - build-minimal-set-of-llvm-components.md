---
id: task-3
title: build minimal set of llvm components
status: To Do
assignee: []
created_date: "2025-12-16 03:35"
labels: []
dependencies: []
---

I _think_ that the current llvm build process (via cmake) builds more components
than extempore actually needs to link against.

For build time efficiency, we should building only the necessary components.
