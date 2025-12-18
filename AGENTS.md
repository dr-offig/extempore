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
ctest --label-regex libs-core -j4      # core library tests
ctest --label-regex libs-external -j4  # external library tests
```

Tests are `.xtm` files in `tests/`. They use `--noaudio` mode automatically.

In addition, building the `aot_external_audio` target (the default) is a pretty
good sign that things are working.

NOTE: this project uses GitHub Actions (in particular the
`.github/workflows/build-and-test.yml` workflow) to build and test on Linux
(x64), macOS (arm64), and Windows (x64).

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

## Running extempore safely

Extempore may send SIGKILL on fatal errors (e.g. LLVM IR compilation failures),
which can terminate the parent process. To isolate crashes when debugging:

```bash
# run with timeout and output capture
(./build/extempore --noaudio --eval "(sys:load \"libs/core/xtmbase.xtm\")" 2>&1 | head -200) &
pid=$!; sleep 30; kill $pid 2>/dev/null; wait $pid 2>/dev/null

# or use timeout command (Linux)
timeout 60 ./build/extempore --noaudio --eval "..."
```

This prevents extempore crashes from killing the agent session.
