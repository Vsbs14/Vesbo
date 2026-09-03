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

### Run source directly

By default, Vesbo uses the tree-walking interpreter:

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
var name is #Vesbo#
var enabled is true
var values is [1, 2, 3]
var missing is none
```

Variables can be reassigned with `set`:

```vsb
set count to count + 1
global var total is 0
```

### Functions and blocks

Functions, conditionals, loops, and error handlers begin a block on the next
line. The block is determined by indentation:

```vsb
ft add(a, b)
	return a + b

ft main()
	output add(2, 3)
```

`gives <type>` may follow a function's parameters as documentation. It is not
currently enforced:

```vsb
ft add(a, b) gives number
	return a + b
```

### Conditions

```vsb
if count equals 5
	output #five#
else if count is greater than 5
	output #more#
else
	output #less#
```

Comparison phrases are `equals`, `does not equal`, `is less than`, `is
greater than`, `is less than or equal`, and `is greater than or equal`.
Logical operators are `AND`, `OR`, and `NOT`.

### Loops

```vsb
loop till done equals true
	output #working#

loop from 1 to 5 as i
	output i

loop through values as value
	output value
```

`loop till` continues until its condition becomes true.

### Arrays

Arrays can be created, indexed, and used in loops:

```vsb
var values is [10, 20, 30]
var first is values[0]

loop through values as value
	output value
```

Array indexes must be integer values.

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
```

Built-in functions are:

- `input()`
- `number(x)`
- `lowercase(x)`
- `trim(x)`
- `length(x)`

Arithmetic follows standard precedence. Parentheses can override it.

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
src/main.c           CLI, bytecode serialization, and program entry point
```

## Current status

Implemented:

- Indentation-based blocks
- Variables and reassignment
- Global variables
- Functions and parameters
- Numbers, strings, booleans, `none`, and arrays
- Array indexing
- Arithmetic and operator precedence
- Comparisons
- `AND`, `OR`, and `NOT`
- Conditionals
- `loop till`
- `loop from ... as`
- `loop through ... as`
- Input/output
- Built-in functions
- `try`/`catch` runtime error handling
- Tree-walking interpretation
- AST-to-bytecode compilation
- Bytecode execution through a virtual machine
- `.vbo` bytecode serialization and loading

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
