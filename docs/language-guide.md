# Language Guide

Vesbo source files use the `.vsb` extension. Blocks are defined by Python-style indentation without braces, colons, or semicolons. Comments begin with `--` and continue to the end of the line.

---

## Values & Reference Semantics

Vesbo supports numbers (`double`), interned strings, booleans (`true` / `false`), arrays, maps, structs, coroutines, and `none`:

```vsb
var count is 5
var ratio = 3.14
var name is "Vesbo"
var title is 'Hello World'
var legacy is #Hash string#
var enabled is true
var values is [1, 2, 3]
var user is { "name": "Alice", "age": 30 }
var missing is none
```

### Assignment Syntax

Variables, array elements, map entries, and struct fields can be declared and reassigned using either English phrasing (`is`, `set ... to ...`) or `=`:

```vsb
var score is 10
set score to score + 5
score = score + 5

global var high_score is 100
```

### Reference Semantics & `copy()`

Composite heap types (**arrays**, **maps**, **struct instances**, and **coroutines**) use **reference semantics** managed by the Mark-Sweep Garbage Collector. Assigning or passing a composite value shares the underlying heap object. Use `copy(x)` to create a shallow clone:

```vsb
var a is [1, 2, 3]
var b is a
b[0] = 99          -- a[0] is now 99

var c is copy(a)
c[0] = 7           -- a[0] remains 99, c[0] is 7
```

---

## Strings & String Interpolation

Strings can be delimited with `"..."`, `'...'`, or `#...#`. Escape sequences (`\n`, `\t`, `\r`, `\"`, `\'`, `\\`, `\#`) are supported. Strings also support indexing (`str[i]`) and iteration (`loop through str as ch`).

Embed expressions directly inside strings using `$"..."` or `f"..."`:

```vsb
var name is "Alice"
var score is 95
output $"Hello, {name}! Your score is {score}."
output f"Next level in {100 - score} points."
```

Use `{{` and `}}` to escape literal braces inside an interpolated string:

```vsb
output $"Coords: {{ {x}, {y} }}"
```

---

## Conditionals & Operators

```vsb
if count equals 5
	output "five"
else if count > 5
	output "more"
else
	output "less"
```

Comparison and logical operators can be written in English phrases or symbols:

| Operation | English Syntax | Symbol Syntax |
| :--- | :--- | :--- |
| Equal | `equals` | `==` |
| Not equal | `does not equal` | `!=` |
| Less than | `is less than` | `<` |
| Greater than | `is greater than` | `>` |
| Less or equal | `is less than or equal` (`to`) | `<=` |
| Greater or equal | `is greater than or equal` (`to`) | `>=` |
| Logical AND | `AND` / `and` | — |
| Logical OR | `OR` / `or` | — |
| Logical NOT | `NOT` / `not` | `!` |

Arithmetic follows standard precedence (`+`, `-`, `*`, `/`, `%`) and can be grouped with parentheses `(...)`.

---

## Loops

```vsb
-- While loop (runs while condition is true)
loop while running equals true
	output "still running"

-- Until loop (runs until condition becomes true)
loop till done equals true
	output "working"

-- Inclusive numeric range loop
loop from 1 to 5 as i
	output i

-- Collection / string iteration
loop through values as value
	output value
```

---

## Functions

Functions are declared with `fnc` (or `ft` for backwards compatibility). If a program defines `fnc main()`, it is automatically called after top-level initialization.

```vsb
fnc add(a, b) gives number
	return a + b

fnc main()
	output add(2, 3)
```

*(Note: `gives <type>` is optional return-type documentation).*

---

## Structs & Methods

Declare custom data types with fields, default field expressions, and methods using `struct`. Methods automatically receive the instance as `self`:

```vsb
struct Vec2
	var x is 0
	var y is 0

	fnc move_by(dx, dy)
		self.x = self.x + dx
		self.y = self.y + dy
		return self

	fnc len_sq()
		return self.x * self.x + self.y * self.y

struct Camera
	var position is Vec2(0, 0)
	var zoom is 1

	fnc pan(dx, dy)
		self.position.move_by(dx, dy)
		return self.position.x + self.position.y

fnc main()
	var v is Vec2(3, 4)
	output v.len_sq()        -- 25
	v.move_by(2, -1)
	output $"({v.x}, {v.y})" -- (5, 3)
```

- **Construction**: Call the struct name with positional arguments (`Vec2(3, 4)`) or rely on default values (`Vec2()`).
- **Introspection**: `type_of(v)` returns `"struct"`, and `struct_name(v)` returns `"Vec2"`.

---

## Arrays & Maps

### Arrays

```vsb
var values is [10, 20, 30]
var first is values[0]

values[1] = 99
push(values, 40)

loop through values as value
	output value
```

### Dictionaries / Maps

Maps use `{ key: value, ... }` syntax and are backed by $O(1)$ open-addressed hash tables with interned string keys while preserving insertion order:

```vsb
var user is { "name": "Alice", "age": 30 }
output user["name"]

user["age"] = 31
user["city"] = "New York"

loop through keys(user) as k
	output $"{k}: {user[k]}"
```

---

## Modules & `import`

Split code across multiple `.vsb` files using `import`:

```vsb
import "std/math"
import "entities/player"

fnc main()
	output clamp(150, 0, 100)
```

- **Path Resolution**: Relative paths resolve relative to the importing file's directory first, then the working directory. The `.vsb` extension is optional.
- **Cycle Detection**: Circular and diamond imports are automatically deduplicated.
- **Self-Contained `.vbo` Bytecode**: Compiling with `-c` bundles all imported modules into a single standalone `.vbo` binary.

---

## Error Handling & Stack Traces

Handle runtime errors with `try` / `catch` and inspect multi-frame `file:line:col` stack traces with `stack_trace()`:

```vsb
fnc main()
	try
		var result is 5 / 0
	catch error
		output $"Caught: {error}"
		output stack_trace()
```
