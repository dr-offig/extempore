# Extempore

Live coding environment for music, audio, and graphics. Scheme interpreter with
xtlang---a statically-typed lisp that compiles to LLVM IR at runtime.

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

Key options: `-DASSETS=ON` (download multimedia assets), `-DBUILD_TESTS=ON`
(default). LLVM 21 is auto-downloaded and built.

## Test

```bash
ctest --label-regex libs-core      # core library tests
ctest --label-regex libs-external  # external library tests
```

Tests are `.xtm` files in `tests/`. They use `--noaudio` mode automatically.

## Structure

| Directory         | Purpose                                               |
| ----------------- | ----------------------------------------------------- |
| `src/`            | C++ runtime (Scheme interpreter, LLVM JIT, audio/OSC) |
| `include/`        | C++ headers                                           |
| `runtime/`        | Bootstrap files (scheme.xtm, LLVM IR bitcode)         |
| `libs/core/`      | Core xtlang standard library                          |
| `libs/external/`  | Bindings to external libs (OpenGL, audio codecs, FFT) |
| `libs/aot-cache/` | AOT-compiled bytecode (auto-generated, don't edit)    |
| `tests/`          | Test files (.xtm)                                     |
| `examples/`       | Example programs                                      |

## Languages

- **C++17**: runtime in `src/` (Scheme.cpp, EXTLLVM.cpp, AudioDevice.cpp)
- **Scheme**: user-facing interpreted language
- **xtlang**: compiled DSL, files use `.xtm` extension, compiles to LLVM IR

## Key files

- `src/Extempore.cpp` --- main entry point
- `src/Scheme.cpp` --- Scheme interpreter
- `src/EXTLLVM.cpp` --- LLVM JIT compilation
- `runtime/scheme.xtm` --- Scheme runtime bootstrap
- `libs/core/test.xtm` --- test harness (`xtmtest-run-tests`, `is?` macro)

## Common tasks

```bash
cmake --build . --target aot        # AOT compile stdlib (faster startup)
cmake --build . --target clean_aot  # rebuild AOT cache
cmake --build . --target xtmdoc     # generate docs
./extempore --noaudio               # run REPL without audio
```
