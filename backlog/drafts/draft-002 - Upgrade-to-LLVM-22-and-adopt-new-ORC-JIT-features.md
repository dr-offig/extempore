---
id: DRAFT-002
title: Upgrade to LLVM 22 and adopt new ORC JIT features
status: Draft
assignee: []
created_date: '2026-02-24 08:39'
labels:
  - llvm
  - jit
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Upgrade Extempore from LLVM 21.1.7 to LLVM 22 (or 23, depending on release timing) and adopt relevant new ORC JIT features. LLVM 22 is currently at RC3.

Extempore already uses ORC JIT (LLJIT) with DynamicLibrarySearchGenerator for host process symbols and manual absoluteSymbols registration for builtins (see src/EXTLLVM.cpp). The upgrade itself should be straightforward since the ORC migration is complete, but several new features are worth adopting.

### Automatic shared library resolver (PR #148410)

Replace the current DynamicLibrarySearchGenerator + manual registerSymbol() setup with the new LibraryResolver. This automatically resolves unresolved symbols against shared libraries with less boilerplate. Evaluate whether it can replace both the DynamicLibrarySearchGenerator and some of the manual symbol registration.

### JIT backtrace symbolication (PR #175099, #175469)

Add limited symbolication of JIT backtraces. Currently a segfault in JIT'd xtlang gives almost no useful information. This would make debugging xtlang crashes significantly easier --- high value for a live-coding environment where crashes during performance are common.

### cloneToContext / cloneExternalModuleToContext (PR #146852)

APIs for cloning modules into a separate ThreadSafeContext. Evaluate whether this improves concurrent compilation --- compiling new xtlang closures on a background thread while the audio thread runs. Extempore already uses ThreadSafeModule so the integration point exists.

### ReOptimizeLayer (PR #173204, #173334)

Evaluate whether the re-optimisation pipeline could improve hot-swapping of xtlang closures, potentially allowing in-place updates to compiled code rather than the current recompile-and-relink approach.

### ELF deinitialise support (PR #175981)

Fixes missing deinitialisation on ELF platform with execution order by priority. Relevant for Linux builds --- check whether Extempore's JIT teardown is affected.

### WaitingOnGraph dependency tracking (PR #163027)

Internal ORC refactor replacing baked-in dependence tracking. Unlikely to require Extempore changes, but worth checking for API breakage during the upgrade.

### Not relevant

- SystemZ JITLink TLS, COFF dlupdate, PowerPC XCOFF --- Extempore targets x86-64 and AArch64 on macOS/Linux only.
- llvm-autojit plugin --- addresses whole-program front-loading, but Extempore compiles incrementally so this isn't a bottleneck.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CMakeLists.txt DEP_LLVM_VERSION bumped to new release
- [ ] #2 Extempore builds and passes tests on macOS (AArch64) and Linux (x86-64)
- [ ] #3 Evaluate LibraryResolver as replacement for current DynamicLibrarySearchGenerator setup
- [ ] #4 Evaluate JIT backtrace symbolication for xtlang crash debugging
- [ ] #5 No regressions in xtlang compilation latency (live-coding responsiveness)
<!-- AC:END -->

## References

- https://github.com/llvm/llvm-project/pull/148410
- https://github.com/llvm/llvm-project/pull/175099
- https://github.com/llvm/llvm-project/pull/146852
- https://github.com/llvm/llvm-project/pull/173204
- https://github.com/llvm/llvm-project/pull/175981
- https://github.com/llvm/llvm-project/pull/163027
- src/EXTLLVM.cpp
- CMakeLists.txt
