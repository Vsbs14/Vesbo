# Standard Library & Built-ins

Vesbo provides a rich set of built-in runtime functions in C (`src/builtins.c`) as well as pure-Vesbo standard library modules under `std/`.

---

## Core Built-in Functions

### I/O, Type Conversion & Introspection

| Function | Description |
| :--- | :--- |
| `output(x)` / `output x` | Print a value to standard output followed by a newline. |
| `input()` | Read a line of text from standard input. |
| `number(x)` | Convert a string or number to a number. |
| `string(x)` | Convert any value to its string representation. |
| `type_of(x)` | Return `"number"`, `"string"`, `"boolean"`, `"none"`, `"array"`, `"map"`, `"struct"`, or `"coroutine"`. |
| `struct_name(x)` | Return the struct type name of a struct instance (or `none` if not a struct). |
| `copy(x)` | Return a shallow copy of an array, map, or struct instance. |

### String Operations

| Function | Description |
| :--- | :--- |
| `length(s)` | Return character count of string `s`. |
| `lowercase(s)` | Convert string `s` to lowercase. |
| `uppercase(s)` | Convert string `s` to uppercase. |
| `trim(s)` | Strip leading and trailing whitespace. |
| `contains(s, sub)` | Return `true` if `s` contains substring `sub`. |
| `replace(s, old, new)` | Replace all occurrences of `old` with `new` in `s`. |
| `split(s, delim)` | Split `s` by `delim` into an array of strings. |
| `join(arr, delim)` | Join array elements into a single string separated by `delim`. |

### Array & Map Operations

| Function | Description |
| :--- | :--- |
| `length(x)` | Return element count of an array or key count of a map. |
| `push(arr, value)` | Append `value` to `arr` in place and return `arr`. |
| `pop(arr)` | Remove and return the last element of `arr`. |
| `contains(collection, item)` | Check if an array contains `item` or a map contains key `item`. |
| `keys(map)` | Return an array of all keys in `map` in insertion order. |
| `values(map)` | Return an array of all values in `map` in insertion order. |
| `has_key(map, key)` | Return `true` if `key` exists in `map`. |
| `remove_key(map, key)` | Remove `key` from `map` and return `true` if removed. |

### Math Built-ins

| Function | Description |
| :--- | :--- |
| `abs(x)` | Absolute value of `x`. |
| `round(x)` | Round `x` to the nearest integer. |
| `floor(x)` | Largest integer less than or equal to `x`. |
| `ceil(x)` | Smallest integer greater than or equal to `x`. |
| `min(a, b)` | Smaller of `a` and `b`. |
| `max(a, b)` | Larger of `a` and `b`. |

### File, System & Time Operations

| Function | Description |
| :--- | :--- |
| `read_file(path)` | Read entire file contents as a string. |
| `write_file(path, content)` | Write string to `path` (creates/overwrites; returns boolean). |
| `append_file(path, content)` | Append string to `path` (returns boolean). |
| `file_exists(path)` | Return `true` if `path` exists. |
| `remove_file(path)` | Delete file at `path` (returns boolean). |
| `system(command)` | Execute a shell command and return its exit code. |
| `env(name)` | Return environment variable `name` (or `none` if unset). |
| `clock()` | Return processor time in seconds for high-resolution benchmarking. |
| `time()` | Return current UNIX timestamp in seconds. |

### Garbage Collector, Profiler & Diagnostics

| Function | Description |
| :--- | :--- |
| `gc_collect()` | Trigger a full Mark-Sweep garbage collection cycle and return live object count. |
| `gc_stats()` / `profile_stats()` | Return a map containing `instructions`, `allocations`, `bytes_allocated`, `live_bytes`, `live_objects`, `collections`, and `objects_freed`. |
| `stack_trace()` | Return the formatted `file:line:col` stack trace string from the most recent error. |

---

## Standard Library Modules (`std/`)

### `std/math`

```vsb
import "std/math"

output PI                  -- 3.14159265358979
output E                   -- 2.71828182845905
output clamp(150, 0, 100)  -- 100
output pow(2, 8)           -- 256
output factorial(5)        -- 120
output gcd(54, 24)         -- 6
output lcm(4, 6)           -- 12
output is_even(10)         -- true
output is_odd(10)          -- false
```

### `std/strings`

```vsb
import "std/strings"

output starts_with("vesbo-lang", "vesbo")  -- true
output ends_with("main.vsb", ".vsb")       -- true
output repeat("=", 10)                     -- "=========="
output pad_left("42", 5, "0")              -- "00042"
output pad_right("hi", 5, " ")             -- "hi   "
```

### `std/lists`

```vsb
import "std/lists"

var nums is [10, 20, 30, 40]
output sum(nums)                   -- 100
output average(nums)               -- 25
output min_item(nums)              -- 10
output max_item(nums)              -- 40
output reverse(nums)               -- [40, 30, 20, 10]
output slice(nums, 1, 3)           -- [20, 30]
output index_of(nums, 30)          -- 2
output count_occurrences(nums, 20) -- 1
```
