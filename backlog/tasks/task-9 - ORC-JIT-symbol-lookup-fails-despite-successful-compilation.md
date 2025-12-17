---
id: task-9
title: ORC JIT symbol lookup fails despite successful compilation
status: To Do
assignee: []
created_date: '2025-12-17 17:30'
updated_date: '2025-12-17 17:30'
labels:
  - llvm
  - jit
  - bug
  - critical
dependencies: []
priority: critical
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After upgrading to LLVM 21 ORC JIT, functions compile and execute correctly but `llvm:get-function-pointer` returns `#f` (not found). This breaks AOT loading because it relies on `mk-ff` which calls `llvm:get-function-pointer` to bind Scheme functions to compiled xtlang code.

The underlying `JIT->lookup(name)` call in `getFunctionAddress()` fails to find symbols that were just added via `JIT->addIRModule()`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 `llvm:get-function-pointer` returns valid cptr for functions compiled via `bind-func`
- [ ] #2 `llvm:get-function-pointer` returns valid cptr for functions loaded from AOT cache via `llvm:compile-ir`
- [ ] #3 Clean build from scratch completes AOT compilation successfully
- [ ] #4 Loading AOT-compiled libraries works without 'non-cptr obj #f' errors
<!-- AC:END -->

## Minimal Reproduction

### What Works

```scheme
;; Direct IR compilation works:
(llvm:compile-ir "define i64 @testfn() { ret i64 42 }")
;; Returns: #<CPTR: ...>

;; bind-func compiles and executes:
(bind-func test_simple (lambda () 42))
(test_simple)  ;; Returns: 42
```

### What Fails

```scheme
;; Function pointer lookup fails immediately after bind-func:
(bind-func test_simple (lambda () 42))
(llvm:get-function-pointer "test_simple")
;; Returns: #f  <-- SHOULD return valid cptr

;; This breaks AOT loading which does:
(mk-ff "hermite_interp_local" (llvm:get-function-pointer "hermite_interp_local_scheme"))
;; ^ When get-function-pointer returns #f, mk-ff tries to use it as a cptr, causing:
;; "Attempting to return a cptr from a non-cptr obj #f"
```

### Full Reproduction

```bash
# Clean build
cd /path/to/extempore
rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc)

# Build succeeds up to 98%, then fails during AOT compilation with:
# Loading xtmaudiobuffer library... Error: evaluating expr: (impc:aot:compile-xtm-file "libs/core/audio_dsp.xtm")
# Attempting to return a cptr from a non-cptr obj #f
```

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Technical Analysis

### Call Flow
1. `bind-func` generates LLVM IR and calls `jitCompile()` in `SchemeFFI.cpp`
2. `jitCompile()` parses IR and calls `EXTLLVM::addTrackedModule()`
3. `addTrackedModule()` calls `JIT->addIRModule()` - this succeeds
4. Later, `llvm:get-function-pointer` calls `EXTLLVM::getFunctionAddress()`
5. `getFunctionAddress()` calls `JIT->lookup(name)` - this fails!

### Key Code Locations

**Symbol Lookup (src/EXTLLVM.cpp:616-624):**
```cpp
uint64_t getFunctionAddress(const std::string& name) {
    if (!JIT) return 0;
    auto sym = JIT->lookup(name);
    if (!sym) {
        llvm::consumeError(sym.takeError());
        return 0;  // Returns 0 when lookup fails
    }
    return sym->getValue();
}
```

**Module Addition (src/EXTLLVM.cpp:648-658):**
```cpp
llvm::Error addTrackedModule(llvm::orc::ThreadSafeModule TSM, const std::vector<std::string>& symbolNames) {
    if (!JIT) return llvm::make_error<llvm::StringError>("JIT not initialized", llvm::inconvertibleErrorCode());
    // Note: symbolNames parameter is ignored!
    if (auto err = JIT->addIRModule(std::move(TSM))) {
        return err;
    }
    return llvm::Error::success();
}
```

### Hypothesis

The ORC JIT's lazy compilation may not be materializing symbols before lookup, or there's a symbol visibility/linkage issue. The symbols might need to be explicitly registered or have different linkage settings.

Possible fixes to investigate:
1. Force materialization of symbols after adding module
2. Check if symbols need explicit export flags
3. Verify JITDylib symbol table contains the symbols
4. Check if there's a name mangling mismatch

### Related LLVM Changes

LLVM 21 ORC JIT has significant API changes from earlier versions. The symbol resolution strategy may have changed.
<!-- SECTION:NOTES:END -->
