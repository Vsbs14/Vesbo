# Vesbo

Vesbo is an indentation-based scripting language written from scratch in C99. Code can run directly through a tree-walking interpreter, execute in-memory on a stack-based bytecode virtual machine (`--vm`), or compile down to standalone `.vbo` (`VSB2`) bytecode files (`-c`).

It includes user-defined structs and methods, stackful coroutines (`coroutine`, `yield`, `resume`), a mark-sweep garbage collector with reference semantics, Raylib 6.0 bindings for 2D games, and a static C library (`libvesbo`) for embedding the VM in C/C++ applications.

## Quick Start

| Task | Windows | Linux / macOS |
| :--- | :--- | :--- |
| Build `vesbo` + `libvesbo` | `build.bat` | `make` |
| Build without Raylib | `build.bat --no-raylib` | `make NO_RAYLIB=1` |
| Run test suite | `tests\run_tests.bat` | `./tests/run_tests.sh` |
| Run script (interpreter) | `vesbo.exe script.vsb` | `./vesbo script.vsb` |
| Run script (bytecode VM) | `vesbo.exe --vm script.vsb` | `./vesbo --vm script.vsb` |
| Compile to `.vbo` bytecode | `vesbo.exe -c script.vsb -o out.vbo` | `./vesbo -c script.vsb -o out.vbo` |
| Run compiled `.vbo` | `vesbo.exe out.vbo` | `./vesbo out.vbo` |
| Interactive REPL | `vesbo.exe` | `./vesbo` |
| Disassemble bytecode | `vesbo.exe -d script.vsb` | `./vesbo -d script.vsb` |
| Run with VM profiler | `vesbo.exe --profile --vm script.vsb` | `./vesbo --profile --vm script.vsb` |
| Run Breakout game | `games\breakout\run.bat` | `./games/breakout/run.sh` |

## CLI Reference

| Flag | Long Option | Description | Example |
| :--- | :--- | :--- | :--- |
| *(none)* | *(none)* | Start interactive REPL | `./vesbo` |
| `<file.vsb>` | *(none)* | Run `.vsb` source in the tree-walking interpreter | `./vesbo program.vsb` |
| `<file.vbo>` | *(none)* | Run compiled `.vbo` bytecode in the stack VM | `./vesbo program.vbo` |
| *(none)* | `--vm` | Compile and run `.vsb` source directly in the bytecode VM | `./vesbo --vm program.vsb` |
| `-c` | `--compile` | Compile `.vsb` source into a `.vbo` bytecode file | `./vesbo -c program.vsb` |
| `-o <file>` | `--output <file>` | Output path when compiling with `-c` | `./vesbo -c program.vsb -o out.vbo` |
| `-d` | `--disassemble` | Disassemble `.vsb` source or `.vbo` bytecode | `./vesbo -d program.vsb` |
| `-i` | `--repl` | Start interactive REPL explicitly | `./vesbo -i` |
| *(none)* | `--gc-stress` | Run a mark-sweep GC cycle before every allocation | `./vesbo --gc-stress program.vsb` |
| *(none)* | `--profile` | Print instruction, allocation, and GC profile at exit | `./vesbo --profile --vm program.vsb` |
| `-v` | `--version` | Print version | `./vesbo -v` |
| `-h` | `--help` | Print usage | `./vesbo -h` |

## Interactive REPL

Run `vesbo` with no arguments (or `-i`) to start an interactive session:

```text
Vesbo 0.2.0 Interactive REPL
Type code to evaluate. Type 'help' for examples or 'exit' to quit.

>>> 21 * 2
42
>>> var message is "hello world"
>>> uppercase(message)
HELLO WORLD
>>> fnc add(a, b)
.....     return a + b
..... 
>>> add(10, 32)
42
>>> exit
```

- Expressions evaluate and print automatically.
- Variables, structs, and functions persist across inputs.
- Multi-line blocks (`fnc`, `struct`, `if`, `loop`, `try`) continue until an empty line is entered.

## Sections

- [Syntax & Types](language-guide.md) — Variables, reference semantics (`copy()`), strings, control flow, functions, structs, arrays, maps, `import`, and `try`/`catch`.
- [Coroutines](coroutines.md) — Stackful coroutines (`coroutine`, `yield`, `resume`) and per-frame game scripting patterns.
- [Standard Library](stdlib.md) — Built-in runtime functions and `std/math`, `std/strings`, and `std/lists`.
- [2D Graphics & Raylib](game-engine.md) — Window management, 2D drawing primitives, input handling, and collision checks.
- [C Embedding & VM](embedding.md) — Embedding `libvesbo` (`include/vesbo.h`) in C/C++ applications and `VSB2` bytecode format.
- [Heap & Mark-Sweep GC](gc_design.md) — `Obj` header layout, string interning, and garbage collector design.
