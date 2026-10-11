# C Embedding (`libvesbo`) & Bytecode VM

Building Vesbo produces a static library (`libvesbo.lib` on Windows MSVC, `libvesbo.a` on Linux, macOS, and MinGW/Clang) alongside the `vesbo` CLI executable. External C/C++ applications and custom game engines can embed isolated `VesboVM` instances via `include/vesbo.h`.

---

## Embedding Quick Example

```c
#include "vesbo.h"
#include <stdio.h>

static Value host_multiply(VesboVM *vm, int argc, Value *argv) {
    if (argc < 2 || argv[0].type != VAL_NUMBER || argv[1].type != VAL_NUMBER) {
        vesbo_error(vm, "host_multiply expects 2 numbers");
        return make_none_value();
    }
    return make_number_value(argv[0].as.number * argv[1].as.number);
}

int main(void) {
    VesboVM *vm = vesbo_new();
    vesbo_register_native(vm, "host_multiply", host_multiply);

    if (!vesbo_load_file(vm, "script.vsb")) {
        fprintf(stderr, "Error: %s\n%s\n", vesbo_last_error(vm), vesbo_last_stack_trace(vm));
        vesbo_free(vm);
        return 1;
    }

    /* Spawn and tick a script coroutine once per frame */
    Value co = vesbo_coroutine(vm, "tick_entity", 0, NULL);
    Value yielded;
    while (vesbo_resume(vm, co, &yielded) == 1) {
        /* Coroutine yielded a frame value */
    }

    vesbo_free(vm);
    return 0;
}
```

---

## `include/vesbo.h` API Reference

| C Function | Description |
| :--- | :--- |
| `VesboVM *vesbo_new(void)` | Allocate and initialize a fresh `VesboVM` context. |
| `void vesbo_free(VesboVM *vm)` | Destroy a `VesboVM` instance and release its compiled chunk and stack buffers. |
| `int vesbo_load_string(VesboVM *vm, const char *source, const char *name)` | Compile and execute top-level definitions from a C string. Returns `1` on success, `0` on error. |
| `int vesbo_load_file(VesboVM *vm, const char *path)` | Read, compile, and execute top-level definitions from a `.vsb` file. |
| `void vesbo_register_native(VesboVM *vm, const char *name, VesboNativeFn fn)` | Register a C callback (`Value (*)(VesboVM *, int, Value *)`) callable from Vesbo scripts. |
| `int vesbo_call(VesboVM *vm, const char *func_name, int argc, Value *argv, Value *out)` | Invoke a Vesbo function by name and store its return value in `*out`. |
| `Value vesbo_coroutine(VesboVM *vm, const char *func_name, int argc, Value *argv)` | Create a first-class suspended `VAL_COROUTINE` from a script function. |
| `int vesbo_resume(VesboVM *vm, Value coro, Value *out)` | Resume a coroutine (`1` = yielded, `0` = returned/dead, `-1` = error). |
| `int vesbo_resume_with(VesboVM *vm, Value coro, Value arg, Value *out)` | Resume a coroutine, sending `arg` as the result of the suspended `yield`. |
| `int vesbo_get_global(VesboVM *vm, const char *name, Value *out)` | Read a global variable from the VM by name. |
| `int vesbo_set_global(VesboVM *vm, const char *name, Value val)` | Set a global variable in the VM by name. |
| `void vesbo_error(VesboVM *vm, const char *message)` | Signal a runtime error from inside a native C callback. |
| `const char *vesbo_last_error(VesboVM *vm)` | Return the most recent error message on `vm`. |
| `const char *vesbo_last_stack_trace(VesboVM *vm)` | Return the formatted `file:line:col` stack trace on `vm`. |

See `tests/embed_host_test.c` and `tests/embed_script.vsb` for a complete working example verified in the automated test suite.

---

## Execution Pipeline & Bytecode (`VSB2`)

Vesbo shares a single lexer/parser front end and unified `Obj` heap across two execution engines:

```text
                      ┌──> tree-walking interpreter (src/interpreter.c)
source -> lexer -> parser -> AST
                      └──> bytecode compiler (src/codegen.c) -> VSB2 bytecode -> stack VM (src/vm.c)
```

### Compiling and Disassembling Bytecode

Compile any `.vsb` program (and all of its `import` dependencies) into a single self-contained `.vbo` file:

```bash
./vesbo -c program.vsb -o program.vbo
./vesbo program.vbo
```

Inspect compiled bytecode instructions, constant pools, struct metadata, and function offsets with `-d` / `--disassemble`:

```bash
./vesbo -d program.vsb
```

The `VSB2` binary format stores:
- Bytecode instruction stream
- Numeric and interned string constant pools
- Function metadata (`name`, `code_offset`, `param_count`)
- Slot-indexed global variable names
- `StructDef` field defaults and method dispatch tables
- Run-length / instruction source location tables (`file:line:col`) for accurate runtime stack traces
