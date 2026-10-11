# Vesbo

**Vesbo** is an English-readable programming language, stack-based bytecode virtual machine, embeddable C runtime (`libvesbo`), and 2D game scripting engine written in C99.

📖 **[Read the Full Documentation →](https://vsbs14.github.io/Vesbo/)**

---

## Highlights

- **Readable Indentation-Based Syntax**: Expressive English keywords (`is`, `equals`, `loop through`) mixed seamlessly with standard operators (`=`, `==`, `<=`) and string interpolation (`$"Hello, {name}!"`).
- **Dual Execution Engine**: Run `.vsb` source directly in the tree-walking interpreter, execute in-memory on the stack VM (`--vm`), or compile self-contained `.vbo` (`VSB2`) bytecode binaries (`-c`).
- **Built for Game Scripting**:
  - **First-Class Stackful Coroutines** (`coroutine`, `yield`, `resume`) for per-frame entity behaviors, AI loops, and cutscenes.
  - **Structs & Methods** (`struct`, `self`) with default field values and dot mutation.
  - **2D Game Runtime (Raylib 6.0)** with built-in windowing, 2D rendering, keyboard/mouse input, and collision detection.
- **Deterministic Mark-Sweep GC**: Unified `Obj` heap with reference semantics for arrays, maps, structs, and coroutines, interned strings, and `copy()` for explicit shallow cloning.
- **Embeddable C API (`libvesbo`)**: Link `libvesbo.a` / `libvesbo.lib` into any C/C++ host or custom game engine via [`include/vesbo.h`](include/vesbo.h).

---

## A Taste of Vesbo

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
		yield $"Tick {tick}: ({pos.x}, {pos.y})"
	return "patrol complete"

fnc main()
	var pos is Vec2(10, 20)
	var co is coroutine(patrol_task, pos, 5)

	loop while is_alive(co)
		output resume(co)
```

---

## Quick Start

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
| **Play Breakout game** | `games\breakout\run.bat` | `./games/breakout/run.sh` |

### Platform Prerequisites

- **Windows**: MSVC (`cl`), MinGW-w64 (`gcc`), or LLVM (`clang`). Raylib 6.0 is pre-vendored in `vendor/raylib/`.
- **Linux**: `gcc` or `clang` + `make`. Optional for 2D graphics: `libraylib-dev` and `pkg-config` (falls back to headless stub automatically if absent).
- **macOS**: Xcode Command Line Tools (`xcode-select --install`). Optional for 2D graphics: `brew install raylib pkg-config`.

---

## Documentation

Full guides and API references live in [`docs/`](docs/) and power the [GitHub Pages documentation site](https://vsbs14.github.io/Vesbo/):

- **[Overview & CLI Reference](docs/README.md)** — CLI flags (`--vm`, `-c`, `-d`, `--profile`, `--gc-stress`) and interactive REPL usage.
- **[Language Guide](docs/language-guide.md)** — Values, reference semantics (`copy()`), strings, loops, functions, structs & methods (`self`), arrays, maps, modules (`import`), and `try`/`catch`.
- **[Coroutines & Game Scripting](docs/coroutines.md)** — Stackful coroutines (`coroutine`, `yield`, `resume`) and per-frame game entity patterns.
- **[2D Game Engine & Raylib API](docs/game-engine.md)** — Windowing, 2D drawing primitives, input queries, collisions, and running Breakout.
- **[Standard Library & Built-ins](docs/stdlib.md)** — All built-in functions and pure-Vesbo `std/math`, `std/strings`, and `std/lists` modules.
- **[C Embedding (`libvesbo`) & Bytecode VM](docs/embedding.md)** — Embedding `VesboVM` in C/C++ applications (`include/vesbo.h`) and `VSB2` bytecode format.
- **[Heap & Mark-Sweep GC Design](docs/gc_design.md)** — Intrusive `Obj` header, string interning, and garbage collector architecture.

---

## Repository Structure

```text
include/     Public C headers (vesbo.h, value.h, vm.h, bytecode.h, ast.h, etc.)
src/         Lexer, parser, AST, GC/heap, interpreter, compiler, VM, REPL, Raylib bindings, C API
std/         Standard library modules written in pure Vesbo (math, strings, lists)
docs/        Documentation site (GitHub Pages) & technical design docs
games/       Games written in pure Vesbo (Breakout)
examples/    Example Vesbo scripts
benchmarks/  AST vs. Bytecode VM performance benchmark suite (benchmark.vsb)
tests/       Automated regression suites, C embedding host test, and test runners
```
