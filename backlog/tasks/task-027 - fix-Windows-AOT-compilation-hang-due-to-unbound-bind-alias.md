---
id: TASK-027
title: fix Windows AOT compilation hang due to unbound bind-alias
status: In Progress
assignee: []
created_date: '2026-02-19 03:31'
labels:
  - bug
  - windows
  - aot
  - ci
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Windows x86_64 CI build on the aarch64 branch (commit 1dd2c72) hangs during the AOT compilation step. The C++ build completes successfully and produces extempore.exe, but the subsequent AOT compilation of libs/base/base.xtm fails and then hangs indefinitely.

The failure sequence in the CI logs (GitHub Actions run 22166027469, job 64093361642):

1. extempore.exe builds successfully
2. AOT compilation begins: Generating D:/a/extempore/extempore/libs/aot-cache/xtmbase.ll
3. function(quit): argument 1 must be: number errors appear (repeated)
4. sys:compile-ll Exceeded maximum runtime --- the LL compilation times out
5. Root cause error: eval: unbound variable: bind-alias during AOT compilation of libs/base/base.xtm
6. After the error, the process hangs for ~1 hour until cancelled (02:28 to 03:29 UTC)

This likely regressed with the recent adhoc alias work (commits 1e9260a7 fix get_native_fptr lookup by adding adhoc alias map and 1dd2c725 add backlog tasks for adhoc alias improvements). The bind-alias symbol is not being made available on Windows before it is needed during AOT compilation.

Linux x86_64 and macOS aarch64 both pass CI on the same commit.

Relevant CI URL: https://github.com/digego/extempore/actions/runs/22166027469/job/64093361642
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bind-alias is defined and available during Windows AOT compilation of libs/base/base.xtm
- [ ] #2 AOT compilation of libs/base/base.xtm completes without hanging on Windows
- [ ] #3 Windows CI job passes on all three platforms (Linux x86_64, macOS aarch64, Windows x86_64)
<!-- AC:END -->
