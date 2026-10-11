---
trigger: always_on
description: Core architectural boundaries, dual-engine parity, GC invariants, and cross-platform build/test rules for the Vesbo language repository.
---

# Vesbo Language Development Rules

## 1. Repository Scope & Architecture
- **Language vs. Engine Separation**: Keep this repository strictly focused on the Vesbo programming language, standard library (`std/`), runtime, and `libvesbo` C embedding API (`include/vesbo.h`, `src/vesbo.c`). External game engines (such as the 2.5D Visual Novel engine) belong in separate repositories that link against `libvesbo`.
- **Dual Execution Path Parity**: Every syntax feature, built-in, struct construct, and coroutine behavior MUST work identically across all three execution modes:
  1. Tree-walking interpreter (`vesbo script.vsb`)
  2. Direct bytecode VM (`vesbo --vm script.vsb`)
  3. Serialized bytecode (`vesbo -c script.vsb -o out.vbo && vesbo out.vbo`)

## 2. Memory Model & Garbage Collector Invariants
- **Reference Semantics**: Arrays (`VAL_ARRAY`), maps (`VAL_MAP`), struct instances (`VAL_STRUCT`), and coroutines (`VAL_COROUTINE`) use heap-allocated `Obj` headers with reference semantics. Only `copy()` performs shallow cloning.
- **Root Safety**: Any C function that allocates multiple heap objects or calls `intern_string()` while holding an unrooted `Value` on the C stack must protect temporary values via `gc_push_root()` / `gc_save_roots()` / `gc_restore_roots()`.
- **Verification**: Always verify runtime changes with `--gc-stress` in both interpreter and `--vm` modes (`vesbo --gc-stress <test.vsb>` and `vesbo --gc-stress --vm <test.vsb>`).

## 3. Cross-Platform Build & Test Hygiene
- **Build & Test Synchronization**: Always keep `build.bat` and `Makefile`, as well as `tests/run_tests.bat` and `tests/run_tests.sh`, in lockstep when adding source files, targets, or tests.
- **Windows Batch Escaping**: Never place unescaped parentheses `(` or `)` inside `echo` statements within parenthesized `if (...)` or `for (...)` blocks in `.bat` files; always escape them as `^(` and `^)`.
- **POSIX Path Safety**: On Linux/macOS, use `realpath(path, NULL)` or `PATH_MAX` (`4096`-byte) buffers rather than `1024`-byte stack buffers to avoid glibc `_FORTIFY_SOURCE` (`__realpath_chk`) aborts.
- **No Tracked Binaries**: Never commit compiled executables (`vesbo`, `*.exe`, `*.lib`, `*.a`) or temporary `.vbo` test artifacts to git.
