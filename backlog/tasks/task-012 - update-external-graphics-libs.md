---
id: task-012
title: update external graphics libs
status: To Do
assignee: []
created_date: "2025-12-18 00:35"
labels: []
dependencies: []
---

On this aarch64 branch we've updated all the versions for the "external audio"
libs that CMakeLists.txt pulls in, but not for the graphics ones.

Partially that's because I suspect there's some bit-rot there, and I don't want
to hold up the release just to fix the (not so essential) graphics stuff. And
it's even more fragile because the "xtlang header" files (anything with a
`bind-lib` in it) are manually generated, so if the C APIs for the deps change
then the xtlang headers need to change too, but there's no way of running it
short of running the tests and a) making sure it doesn't crash, and b)
visually/aurally inspecting the output (in the case of graphics/audio libs).

Anyway, with those caveats aside, it might be worth _trying_ to update the
graphics libs and see how we go.
