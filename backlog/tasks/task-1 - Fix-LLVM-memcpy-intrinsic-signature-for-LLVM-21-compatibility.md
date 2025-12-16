---
id: task-1
title: Fix LLVM memcpy intrinsic signature for LLVM 21 compatibility
status: In Progress
assignee: []
created_date: '2025-12-16 02:13'
updated_date: '2025-12-16 02:14'
labels:
  - llvm
  - compiler
  - aarch64
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When running core tests (tests/all-core.xtm), the xtmbase library fails to load with an LLVM IR error due to breaking changes in LLVM's memcpy intrinsic signature between older LLVM versions and LLVM 21. This blocks PR #415 (aarch64 support) from passing core tests.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Update memcpy intrinsic generation to use new LLVM 21 signature: @llvm.memcpy.p0.p0.i64(ptr dest, ptr src, i64 len, i1 isvolatile)
- [ ] #2 Update memmove intrinsic generation to use new LLVM 21 signature if applicable
- [ ] #3 Update memset intrinsic generation to use new LLVM 21 signature if applicable
- [ ] #4 Handle alignment via pointer attributes instead of separate parameter
- [ ] #5 Core tests (tests/all-core.xtm) load xtmbase successfully without IR errors
- [ ] #6 Verify compatibility with both old and new LLVM versions if needed
<!-- AC:END -->
