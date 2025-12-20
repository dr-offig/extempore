# Extempore debugging skill

## Architecture overview

Extempore has three main layers:

1. **C++ runtime** (`src/`): Scheme interpreter, LLVM JIT, audio/OSC
2. **Scheme runtime** (`runtime/`): scheme.xtm, llvmir.xtm, llvmti.xtm
3. **xtlang libraries** (`libs/`): user-facing compiled DSL code

## Compilation paths

### Normal (interactive) compilation

```
llvm:compile-ir
  -> llvm:jit-compile-ir-string (Scheme FFI)
    -> jitCompile() in src/SchemeFFI.cpp
      -> initializeTemplateModule() parses runtime/bitcode.ll once
      -> parseAssemblyInto() of (type defs + user IR)
      -> EXTLLVM::addTrackedModule() (ORC JIT)
      -> EXTLLVM::addModule() (metadata tracking)
```

### AOT compilation

When `*impc:aot:current-output-port*` is set:

```
llvm:compile-ir
  -> impc:compiler:queue-ir-for-compilation
    -> appends to *impc:compiler:queued-llvm-ir-string*

impc:compiler:flush-jit-compilation-queue
  -> llvm:jit-compile-ir-string with accumulated IR
```

## Startup sequence

1. C++ `main()` in Extempore.cpp
2. SchemeProcess ctor loads `runtime/init.xtm`
3. SchemeProcess task loads `runtime/scheme.xtm`, `runtime/llvmti.xtm`,
   `runtime/llvmir.xtm`
4. Primary process compiles `runtime/init.ll` via `sys:compile-init-ll`
5. If `EXT_LOADBASE` is true (default), loads `libs/base/base.xtm`
6. `base.xtm` triggers AOT cache loading via
   `impc:aot:insert-header`/`impc:aot:import-ll`
7. AOT cache files (e.g. `libs/aot-cache/base.xtm`) call `llvm:compile-ir` with
   `.ll` files

## Key flags

- `--nobase`: Skip loading base library (useful for debugging JIT in isolation)
- `--noaudio`: Disable audio (required for headless/CI testing)
- `--batch "expr"`: Batch mode (no server, single process); exits only if the
  expression calls `(quit ...)`

## Symbol tracking

`EXTLLVM::addModule()` populates `sGlobalMap` with function/global pointers:

- Key: symbol name (string)
- Value: pointer to `llvm::GlobalValue` in the metadata module clone

`EXTLLVM::getFunction()` / `EXTLLVM::getGlobalValue()` look up symbols in this
map.

## Common issues

### Type definitions

AOT-compiled `.ll` files reference types like `%mzone`, `%clsvar` defined in
`runtime/bitcode.ll`. These must be available when parsing user IR.

### Windows CRLF

Regex-based IR parsing fails on Windows due to CRLF line endings. Use
line-by-line parsing with explicit CR stripping.

### Symbol not found after compilation

Check that:

1. Module was added to ORC JIT successfully
2. `EXTLLVM::addModule()` was called with the metadata clone
3. Symbol name matches exactly (including mangling like `_adhoc_`, `_poly_`)

## Debugging commands

```scheme
;; List all modules
(llvm:list-modules)

;; Print all modules
(llvm:print)

;; Check if function exists
(llvm:get-function "function_name")

;; Print specific function
(llvm:print-function "prefix")
```

## Testing in isolation

```bash
# Skip base library to test JIT directly
./extempore --noaudio --nobase --batch "(begin (llvm:jit-compile-ir-string \"define i64 @test() { ret i64 42 }\") (println (llvm:get-function \"test\")) (quit 0))"

# Test AOT cache loading
./extempore --noaudio --nobase --batch "(begin (llvm:compile-ir (sys:slurp-file \"libs/aot-cache/xtmbase.ll\")) (quit 0))"
```

## C++ debug output

Use `printf()` with `fflush(stdout)` rather than `std::cerr` - extempore may
redirect stderr.

## Key files

| File                   | Purpose                                             |
| ---------------------- | --------------------------------------------------- |
| `src/SchemeFFI.cpp`    | `jitCompile()` - main JIT entry point               |
| `src/EXTLLVM.cpp`      | `addModule()`, `getGlobalValue()` - symbol tracking |
| `src/ffi/llvm.inc`     | Scheme FFI bindings for LLVM functions              |
| `runtime/llvmir.xtm`   | `llvm:compile-ir`, compilation queue                |
| `runtime/llvmti.xtm`   | Type inference, AOT compilation                     |
| `runtime/bitcode.ll`   | Base type definitions (`%mzone`, `%clsvar`)         |
| `libs/aot-cache/*.ll`  | Pre-compiled LLVM IR                                |
| `libs/aot-cache/*.xtm` | Scheme stubs that load `.ll` files                  |
