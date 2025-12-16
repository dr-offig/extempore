---
id: task-4
title: Ensure llvm-as is built and available for AOT compilation
status: To Do
assignee: []
created_date: '2025-12-16 06:21'
labels:
  - build
  - aot
  - llvm
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
AOT (ahead-of-time) compilation in Extempore requires the llvm-as tool to assemble LLVM IR files. Currently, llvm-as is not being built as part of the LLVM build in Extempore's CMake setup, causing AOT compilation to fail silently - the process runs but produces no output in libs/aot-cache/. The LLVM source is fetched to build/_deps/llvm-src/ but the llvm-as tool binary is not compiled.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 llvm-as binary is built during LLVM compilation
- [ ] #2 llvm-as is accessible to Extempore's AOT compilation process
- [ ] #3 AOT compilation successfully produces output files in libs/aot-cache/
- [ ] #4 Changes are limited to CMake configuration for LLVM build
<!-- AC:END -->
