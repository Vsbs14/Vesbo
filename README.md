# Vesbo

Vesbo is an English-readable programming language written in C. Its syntax
uses Python-style indentation without braces, colons, or semicolons.

The current implementation is a tree-walking interpreter. The long-term goal
is to compile Vesbo to bytecode and eventually to native code.

## Build

### Windows

Install MinGW-w64 GCC, Visual Studio Build Tools, or LLVM Clang. From the
project directory, run:

```bat
build.bat
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

Built-in functions are `input()`, `number(x)`, `lowercase(x)`, `trim(x)`, and
`length(x)`. Array indexes must be integer values:

```vsb
var first is [10, 20][0]
```

Arithmetic follows standard precedence. Parentheses can override it.

## Project layout

```text
include/   Public C interfaces and data structures
src/       Lexer, parser, AST, runtime, built-ins, and CLI
examples/  Vesbo programs
tests/     Regression program and Windows smoke-test runner
```

The current pipeline is:

```text
source -> lexer -> parser -> AST -> tree-walking interpreter
```

The planned compiler pipeline is:

```text
source -> lexer -> parser -> AST -> bytecode compiler -> virtual machine
```

## Current status

Implemented features include indentation-based blocks, functions, variables,
arrays, indexing, arithmetic, comparisons, conditionals, loops, input/output,
built-ins, and catchable runtime errors. The project builds on Windows with
MSVC, MinGW GCC, or Clang, and on Unix-like systems with GCC or Clang.

The next major milestone is bytecode compilation. Native code generation can
follow once the bytecode semantics and test suite are stable.