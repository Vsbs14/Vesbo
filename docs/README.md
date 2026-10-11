# Vesbo Documentation

Welcome to the official documentation for **Vesbo** — an English-readable programming language, stack-based bytecode virtual machine, C embedding library (`libvesbo`), and 2D game scripting runtime written in C99.

---

## Core Capabilities

- **Dual Execution Engine**: Run `.vsb` scripts directly with the tree-walking interpreter or compile them to `VSB2` bytecode for fast stack-VM execution (`--vm` or `.vbo` binaries).
- **Game Scripting First**: Built-in **stackful coroutines** (`coroutine`, `yield`, `resume`), **user-defined structs & methods** (`struct`, `self`), and **Raylib 6.0 2D graphics/input/collision bindings**.
- **Deterministic Mark-Sweep GC**: Unified `Obj` heap with reference semantics for arrays, maps, structs, and coroutines, interned strings, and `copy()` for explicit shallow cloning.
- **Embeddable C Runtime (`libvesbo`)**: Link `libvesbo.a` / `libvesbo.lib` into any C/C++ application or custom game engine via `include/vesbo.h`.

---

## Quick Start Commands

| Task | Windows | Linux / macOS |
| :--- | :--- | :--- |
| **Build (`vesbo` + `libvesbo`)** | `build.bat` | `make` |
| **Build without Raylib** | `build.bat --no-raylib` | `make NO_RAYLIB=1` |
| **Run test suite** | `tests\run_tests.bat` | `./tests/run_tests.sh` |
| **Run script (interpreter)** | `vesbo.exe script.vsb` | `./vesbo script.vsb` |
| **Run script (bytecode VM)** | `vesbo.exe --vm script.vsb` | `./vesbo --vm script.vsb` |
| **Compile to bytecode (`.vbo`)** | `vesbo.exe -c script.vsb -o out.vbo` | `./vesbo -c script.vsb -o out.vbo` |
| **Run compiled `.vbo`** | `vesbo.exe out.vbo` | `./vesbo out.vbo` |
| **Interactive REPL** | `vesbo.exe` | `./vesbo` |
| **Disassemble bytecode** | `vesbo.exe -d script.vsb` | `./vesbo -d script.vsb` |
| **Run with VM profiler** | `vesbo.exe --profile --vm script.vsb` | `./vesbo --profile --vm script.vsb` |
| **Play Breakout game** | `games\breakout\run.bat` | `./games/breakout/run.sh` |

---

## CLI Options Reference

| Flag | Long Option | Description | Example |
| :--- | :--- | :--- | :--- |
| *(none)* | *(none)* | Launch interactive REPL session | `./vesbo` |
| `<file.vsb>` | *(none)* | Execute `.vsb` source with tree-walking interpreter | `./vesbo program.vsb` |
| `<file.vbo>` | *(none)* | Execute compiled `.vbo` bytecode with stack VM | `./vesbo program.vbo` |
| *(none)* | `--vm` | Compile and run `.vsb` source directly in the bytecode VM | `./vesbo --vm program.vsb` |
| `-c` | `--compile` | Compile `.vsb` source into `.vbo` bytecode file | `./vesbo -c program.vsb` |
| `-o <file>` | `--output <file>` | Specify output bytecode file path (with `-c`) | `./vesbo -c program.vsb -o out.vbo` |
| `-d` | `--disassemble` | Disassemble `.vsb` source or `.vbo` bytecode chunk | `./vesbo -d program.vsb` |
| `-i` | `--repl` | Launch interactive REPL session explicitly | `./vesbo -i` |
| *(none)* | `--gc-stress` | Run garbage collection before every heap allocation | `./vesbo --gc-stress program.vsb` |
| *(none)* | `--profile` | Print instruction, allocation, and GC profile at exit | `./vesbo --profile --vm program.vsb` |
| `-v` | `--version` | Display Vesbo version | `./vesbo -v` |
| `-h` | `--help` | Display help and usage message | `./vesbo -h` |

---

## Interactive REPL

Launch the interactive Read-Eval-Print Loop by running `vesbo` with no arguments (or `-i` / `--repl`):

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

- Expressions automatically print their evaluated result.
- Variables, structs, and functions persist across turns.
- Multi-line blocks (`fnc`, `struct`, `if`, `loop`, `try`) auto-indent until an empty line is entered.

---

## Documentation Sections

- **[Language Guide](language-guide.md)** — Variables, strings, control flow, functions, structs & methods, arrays, maps, modules, and error handling.
- **[Coroutines & Game Scripting](coroutines.md)** — Writing stateful tasks, frame-by-frame entity behaviors, and generators with `coroutine`, `yield`, and `resume`.
- **[Standard Library & Built-ins](stdlib.md)** — Complete reference for built-in functions and `std/math`, `std/strings`, and `std/lists`.
- **[2D Game Engine & Raylib API](game-engine.md)** — Window management, 2D rendering, input handling, and collision helpers.
- **[C Embedding (`libvesbo`) & VM](embedding.md)** — Embedding Vesbo in C/C++ hosts, native callbacks, and bytecode internals.
- **[Heap & Mark-Sweep GC Design](gc_design.md)** — Deep dive into `Obj` representation, string interning, and garbage collection.
