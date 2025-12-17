---
id: task-9
title: fix GH actions caching issues
status: To Do
assignee: []
created_date: "2025-12-17 05:55"
labels: []
dependencies: []
---

Look at recent runs - the LLVM build isn't cached, which costs lots of time.

See this info which might be relevant:
https://github.com/actions/cache/tree/main/save#always-save-cache
