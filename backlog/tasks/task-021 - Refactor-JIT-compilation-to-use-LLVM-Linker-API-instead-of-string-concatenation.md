---
id: task-021
title: >-
  Refactor JIT compilation to use LLVM Linker API instead of string
  concatenation
status: To Do
assignee: []
created_date: '2025-12-19 22:57'
labels:
  - llvm
  - jit
  - refactoring
  - maintainability
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The current `jitCompile()` function in `src/SchemeFFI.cpp` uses regex-driven string munging to compose LLVM IR modules. This involves:

1. Parsing `runtime/bitcode.ll` and caching it as a string
2. Extracting symbols via regex (`sGlobalSymRegex`, `sDefineSymRegex`, etc.)
3. Building declaration strings by querying existing LLVM functions and formatting types
4. Concatenating strings: `sInlineString + userTypeDefs + externalGlobals + externalLibFunctions + declarations + newIR`
5. Parsing the combined string as a new module

This approach is fragile, hard to maintain, and performs redundant parsing.

**Goal:** Replace string concatenation with LLVM's module linking APIs for cleaner, more robust JIT compilation.

**Key data structures to refactor:**
- `sUserTypeDefs` - map of user-defined type names to definitions
- `sExternalGlobals` - map of external global names to types  
- `sExternalLibFunctions` - map of bind-lib function names to declarations
- `sInlineString` / `sInlineBitcode` - cached base runtime

**Proposed approach:**
1. Parse `runtime/bitcode.ll` once into a template module
2. For each compilation, clone the template module
3. Use `llvm::Linker` to merge the new IR module into the cloned template
4. Add external declarations via LLVM APIs (`Module::getOrInsertFunction`, `Module::getOrInsertGlobal`) instead of string formatting
5. Remove regex caches and string concatenation logic
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 jitCompile() uses llvm::Linker instead of string concatenation for module composition
- [ ] #2 External declarations added via LLVM APIs, not string formatting
- [ ] #3 Regex caches (sUserTypeDefs, sExternalGlobals, sExternalLibFunctions) replaced with structured data or eliminated
- [ ] #4 All existing tests pass (libs-core, libs-external)
- [ ] #5 aot_external_audio target builds successfully
- [ ] #6 No performance regression in JIT compilation time
<!-- AC:END -->
