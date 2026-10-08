# Vesbo

Vesbo is an English-readable programming language written in C. Its syntax
uses Python-style indentation without braces, colons, or semicolons.

Vesbo currently has **two execution paths**:

- A tree-walking interpreter for executing `.vsb` source directly.
- A bytecode compiler and stack-based virtual machine (VM) for compiling and
  executing `.vbo` bytecode files.

The language and runtime are still under active development.

## Build

### Windows

Install one of the following C toolchains:

- MinGW-w64 GCC
- Visual Studio Build Tools
- LLVM Clang

From the project directory, run:

```bat
build.bat
```

Then run a Vesbo source file:

```bat
vesbo.exe examples\array_loops.vsb
```

Run the smoke tests with:

```bat
tests\run_tests.bat
```

### Linux and macOS

With GCC or Clang and `make` installed, run:

```sh
make
./vesbo examples/array_loops.vsb
```

## Running Vesbo

Vesbo source files use the `.vsb` extension.

### Interactive REPL

Start an interactive read-eval-print loop by launching Vesbo with no arguments (or with `-i` / `--repl`):

```sh
./vesbo
# or:
./vesbo -i
```

On Windows:

```bat
vesbo.exe
```

In the REPL, expressions are automatically evaluated and displayed:

```text
Vesbo 0.2.0 Interactive REPL
Type code to evaluate. Type 'help' for examples or 'exit' to quit.

>>> 21 * 2
42
>>> var message is #hello world#
>>> uppercase(message)
HELLO WORLD
>>> [1, 2, 3][0]
1
>>> ft add(a, b)
.....     return a + b
..... 
>>> add(10, 32)
42
>>> exit
```

Features:
- Expressions auto-print their results.
- Variables and declared functions persist across turns.
- Multi-line blocks (`ft`, `if`, `loop`, `try`) auto-indent until an empty line is entered.
- Graceful error recovery: typos and parse errors will not exit the REPL.
- Built-in `help` and `exit`/`quit` commands.

### Run source directly

By default, Vesbo uses the tree-walking interpreter when given a `.vsb` file:

```sh
./vesbo program.vsb
```

On Windows:

```bat
vesbo.exe program.vsb
```

### Compile to bytecode

Use `-c` or `--compile` to compile a Vesbo program into a `.vbo` bytecode file:

```sh
./vesbo -c program.vsb
```

This creates:

```text
program.vbo
```

A different output path can be supplied with `-o` or `--output`:

```sh
./vesbo -c program.vsb -o build/program.vbo
```

### Run bytecode

`.vbo` files are loaded directly by the virtual machine:

```sh
./vesbo program.vbo
```

The same commands work with `vesbo.exe` on Windows.

### Disassemble bytecode

Use `-d` or `--disassemble` to inspect the compiled bytecode instructions, constant pools, and function offsets of either a source file (`.vsb`) or a compiled binary (`.vbo`):

```sh
./vesbo -d program.vsb
./vesbo -d program.vbo
```

Example disassembly output:

```text
========================================
 Bytecode Disassembly: examples/array_loops.vsb
========================================

-- Constant Numbers (9) --
  [0] 4
  [1] 8
...
-- Constant Strings (3) --
  [0] "Total:"
  [1] "Average:"
  [2] "Counting 1 to 5:"

-- Functions (1) --
  [0] <main> (offset: 0005, params: 0)

-- Instructions (264 bytes) --
0000  OP_JUMP              -> 0257
<main> (params: 0):
0005  OP_PUSH_NUMBER       0 (4)
0010  OP_PUSH_NUMBER       1 (8)
...
0081  OP_GET_LOCAL         slot: 3
0086  OP_GET_LOCAL         slot: 2
0091  OP_LENGTH           
0092  OP_LESS_THAN        
0093  OP_JUMP_IF_FALSE     -> 0151
...
0257  OP_CALL              [0] <main> (params: 0)
0262  OP_POP              
0263  OP_HALT             
========================================
```

The current bytecode format begins with the `VSBX` magic header and stores the
compiled instructions, numeric constants, string constants, and function
metadata.

## Language

Vesbo source files use the `.vsb` extension. Comments begin with `--` and
continue to the end of the line.

### Values

The language currently supports numbers, strings, booleans, arrays, and
`none`:

```vsb
var count is 5
var name is "Vesbo"
var title is 'Hello World'
var legacy is #Hash string#
var enabled is true
var values is [1, 2, 3]
var user is { "name": "Alice", "age": 30 }
var missing is none
```

Strings can be delimited using double quotes (`"..."`), single quotes (`'...'`), or hashes (`#...#`). Escape sequences like `\n`, `\t`, `\"`, `\'`, `\\`, and `\#` are supported.

Variables can be reassigned with `set`:

```vsb
set count to count + 1
global var total is 0
```

### Functions and blocks

Functions, conditionals, loops, and error handlers begin a block on the next
line. The block is determined by indentation:

```vsb
fnc add(a, b)
	return a + b

fnc main()
	output add(2, 3)
```

*(Note: `ft` is also supported as an alias for `fnc` for backwards compatibility).*

`gives <type>` may follow a function's parameters as documentation. It is not
currently enforced:

```vsb
fnc add(a, b) gives number
	return a + b
```

### Conditions

```vsb
if count equals 5
	output "five"
else if count > 5
	output "more"
else
	output "less"
```

Comparison operators can be written in English phrases or standard symbols:
- `equals` or `==`
- `does not equal` or `!=`
- `is less than` or `<`
- `is greater than` or `>`
- `is less than or equal` (or `is less than or equal to`) or `<=`
- `is greater than or equal` (or `is greater than or equal to`) or `>=`

Logical operators are `AND`, `OR`, and `NOT`.

### Loops

```vsb
loop while running equals true
	output "still running"

loop till done equals true
	output "working"

loop from 1 to 5 as i
	output i

loop through values as value
	output value
```

- `loop while <cond>` continues executing as long as `<cond>` evaluates to `true`.
- `loop till <cond>` continues executing until `<cond>` becomes `true`.

### Arrays

Arrays can be created, indexed, modified, and used in loops:

```vsb
var values is [10, 20, 30]
var first is values[0]

set values[1] to 99

loop through values as value
	output value
```

Array indexes must be integer values.

### Dictionaries / Maps

Key-value maps are created using `{ key: value, ... }` syntax. Keys and values can be dynamically read and modified:

```vsb
var user is { "name": "Alice", "age": 30 }
output user["name"]

set user["age"] to 31
set user["city"] to "New York"
```

Iterate over dictionary keys or values using `keys()` and `values()`:

```vsb
loop through keys(user) as k
	output k + ": " + string(user[k])
```

### Errors

Runtime errors can be handled with `try` and `catch`:

```vsb
ft main()
	try
		var result is 5 / 0
	catch error
		output error
```

Uncaught errors stop the program and are reported on the command line.

### Input and built-ins

```vsb
output #Hello#
var name is input()
var number_value is number(#42#)
var clean_name is lowercase(trim(name))
var item_count is length([1, 2, 3])
var greeting is uppercase(#hello#)
var as_str is string(123)
var type_name is type_of([1, 2])
```

#### Built-in functions:

- **I/O & Conversion:**
  - `output(x)` — Print value followed by newline
  - `input()` — Read a line of input from standard input
  - `number(x)` — Convert string or number to number
  - `string(x)` — Convert any value to its string representation
  - `type_of(x)` — Return type name (`number`, `string`, `boolean`, `none`, `array`, `map`)

- **String Operations:**
  - `lowercase(s)` — Convert string to lowercase
  - `uppercase(s)` — Convert string to uppercase
  - `trim(s)` — Trim leading and trailing whitespace
  - `contains(collection, item)` — Check if string contains substring, array contains element, or map has key
  - `replace(s, old, new)` — Replace all occurrences of `old` with `new`
  - `split(s, delim)` — Split string into an array of substrings
  - `join(arr, delim)` — Join an array of elements with a delimiter into a string

- **Array & Map Operations:**
  - `length(x)` — Element count of an array, character count of a string, or entry count of a map
  - `push(arr, value)` — Append value to array (mutates and returns array)
  - `pop(arr)` — Remove and return the last element of an array
  - `keys(map)` — Return an array of all string keys in a map
  - `values(map)` — Return an array of all values in a map
  - `has_key(map, key)` — Return boolean indicating if key exists in map
  - `remove_key(map, key)` — Remove key from map (mutates map, returns boolean)

- **Math Operations:**
  - `abs(x)` — Absolute value of a number
  - `round(x)` — Round number to nearest integer
  - `floor(x)` — Largest integer less than or equal to number
  - `ceil(x)` — Smallest integer greater than or equal to number
  - `min(a, b)` — Smaller of two numbers
  - `max(a, b)` — Larger of two numbers

- **File & System Operations:**
  - `read_file(path)` — Read entire file contents into a string
  - `write_file(path, content)` — Write string to a file (creates or overwrites, returns boolean)
  - `append_file(path, content)` — Append string to a file (creates if not existing, returns boolean)
  - `file_exists(path)` — Check if a file exists (returns boolean)
  - `remove_file(path)` — Delete a file (returns boolean)
  - `system(command)` — Execute a shell command and return its exit code
  - `env(name)` — Get the value of an environment variable (or `none` if unset)
  - `clock()` — Returns processor time in seconds for high-precision benchmarking
  - `time()` — Returns current UNIX timestamp in seconds

Arithmetic follows standard precedence (`+`, `-`, `*`, `/`, `%`). Parentheses can override it. Strings also support indexing (`str[i]`) and iteration (`loop through str as ch`). Functions can be called before their definition (forward references).

## Execution architecture

Vesbo has a shared front end and two runtime paths:

```text
                    ┌──> tree-walking interpreter
source -> lexer -> parser -> AST
                    └──> bytecode compiler -> bytecode -> VM
```

The lexer converts source text into tokens. The parser builds an abstract
syntax tree (AST). From there, the AST can either be executed directly by the
interpreter or compiled into bytecode for the VM.

The VM is a stack-based virtual machine with support for:

- Number, string, boolean, and `none` constants
- Arrays and indexing
- Local and global variables
- Arithmetic and comparisons
- Logical operations
- Conditional jumps and loops
- Function calls and returns
- `try`/`catch` error handling
- Built-in functions
- Program termination

Function metadata is stored alongside the bytecode so compiled `.vbo` files
can be loaded and executed without the original `.vsb` source.

## Project layout

```text
include/   Public C interfaces and data structures
src/       Lexer, parser, AST, runtime, bytecode compiler, VM, and CLI
examples/  Vesbo programs and compiled bytecode examples
tests/     Regression program and Windows smoke-test runner
```

Important source files include:

```text
src/lexer.c          Tokenization
src/parser.c         Source -> AST
src/ast.c            AST structures and management
src/interpreter.c    Tree-walking interpreter
src/bytecode.c       Bytecode chunk and constant-pool management
src/codegen.c        AST -> bytecode compiler
src/vm.c             Bytecode virtual machine
src/disassemble.c    Bytecode disassembler and constant inspector
src/repl.c           Interactive REPL session manager
src/main.c           CLI, bytecode serialization, and program entry point
```

## Current status

Implemented:

- Indentation-based blocks
- Variables and reassignment
- Global variables
- Functions and parameters (`fnc` and `ft`)
- Numbers, strings (`"..."`, `'...'`, `#...#` with escape sequences), booleans, `none`, and arrays
- Array indexing
- Arithmetic and operator precedence
- Comparisons (English phrases and symbols: `<`, `<=`, `>`, `>=`, `==`, `!=`)
- `AND`, `OR`, and `NOT`
- Conditionals
- `loop while` and `loop till`
- `loop from ... as`
- `loop through ... as`
- Input/output
- Built-in functions
- `try`/`catch` runtime error handling
- Tree-walking interpretation
- AST-to-bytecode compilation
- Bytecode execution through a virtual machine
- `.vbo` bytecode serialization and loading
- Interactive REPL with expression evaluation and multiline input
- Bytecode disassembler (`-d`, `--disassemble`) for `.vsb` and `.vbo` files

The bytecode VM and compiler are now functional, but the project is not yet a
stable language implementation. The bytecode format, runtime behavior, error
handling, and language semantics may change as development continues.

Native code generation is a possible future direction, but it is **not
currently implemented**.

## Examples

The repository includes several example programs:

```text
examples/array_loops.vsb
examples/calculator.vsb
examples/try_catch_test.vsb
```

There are also compiled `.vbo` examples in the repository.

For a quick test of the bytecode pipeline:

```sh
./vesbo -c examples/array_loops.vsb -o /tmp/array_loops.vbo
./vesbo /tmp/array_loops.vbo
```

On Windows:

```bat
vesbo.exe -c examples\array_loops.vsb -o array_loops.vbo
vesbo.exe array_loops.vbo
```
