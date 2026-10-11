# Vesbo

Vesbo is an indentation-based scripting language written from scratch in C99. It includes a lexer, recursive-descent parser, tree-walking interpreter, bytecode compiler, stack-based virtual machine, mark-sweep garbage collector, C embedding API (`libvesbo`), and Raylib 2D bindings for writing games.

**[Documentation (vsbs14.github.io/Vesbo) →](https://vsbs14.github.io/Vesbo/)**

## Example

```vsb
struct Vec2
	var x is 0
	var y is 0

	fnc move_by(dx, dy)
		self.x = self.x + dx
		self.y = self.y + dy
		return self

fnc patrol_task(pos, step)
	loop from 1 to 3 as tick
		pos.move_by(step, 0)
		yield $"tick {tick}: ({pos.x}, {pos.y})"
	return "done"

fnc main()
	var pos is Vec2(10, 20)
	var co is coroutine(patrol_task, pos, 5)

	loop while is_alive(co)
		output resume(co)
```

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
| Run Breakout game | `games\breakout\run.bat` | `./games/breakout/run.sh` |

### Building

- **Windows**: Requires MSVC (`cl`), MinGW-w64 (`gcc`), or LLVM (`clang`). Raylib 6.0 headers and libraries are pre-vendored in `vendor/raylib/`.
- **Linux**: Requires `gcc` or `clang` and `make`. For 2D graphics, install `libraylib-dev` and `pkg-config` (if absent, `make` falls back to a headless stub automatically).
- **macOS**: Requires Xcode Command Line Tools (`xcode-select --install`). For 2D graphics, run `brew install raylib pkg-config`.

## Documentation

The full language and runtime reference is hosted at **[vsbs14.github.io/Vesbo](https://vsbs14.github.io/Vesbo/)** and lives in [`docs/`](docs/):

- [Getting Started & CLI Reference](docs/README.md) — CLI flags (`--vm`, `-c`, `-d`, `--profile`, `--gc-stress`) and REPL usage.
- [Language Guide](docs/language-guide.md) — Variables, reference semantics (`copy()`), strings, control flow, functions, structs & methods (`self`), arrays, maps, `import`, and `try`/`catch`.
- [Coroutines](docs/coroutines.md) — Stackful coroutines (`coroutine`, `yield`, `resume`) and per-frame entity state machines.
- [2D Graphics & Raylib API](docs/game-engine.md) — Window loop, 2D primitives, input handling, collisions, and Breakout.
- [Standard Library & Built-ins](docs/stdlib.md) — Built-in runtime functions and `std/math`, `std/strings`, and `std/lists`.
- [C Embedding (`libvesbo`) & VM](docs/embedding.md) — Embedding `VesboVM` in C/C++ applications (`include/vesbo.h`) and `VSB2` bytecode layout.
- [Heap & Mark-Sweep GC Design](docs/gc_design.md) — `Obj` representation, string interning, and garbage collection.

## Directory Layout

```text
include/     C headers (vesbo.h, value.h, vm.h, bytecode.h, ast.h, etc.)
src/         Lexer, parser, AST, GC/heap, interpreter, compiler, VM, REPL, Raylib bindings, C API
std/         Standard library modules written in Vesbo (math, strings, lists)
docs/        Documentation site (GitHub Pages) and GC design notes
games/       Breakout game written in Vesbo
examples/    Sample scripts
benchmarks/  Interpreter vs. bytecode VM benchmark suite (benchmark.vsb)
tests/       Automated regression tests, C embedding host test, and test runners
```
