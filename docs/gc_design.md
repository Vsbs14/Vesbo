# Vesbo Heap Object Model & Garbage Collector Design (L1)

## 1. Overview
Vesbo uses a unified heap object representation (`Obj`) managed by a deterministic, stop-the-world **Mark-Sweep Garbage Collector**. Strings are immutable and interned globally, while composite types (**arrays**, **maps**, **struct instances**, and **coroutines**) use **reference semantics** with an explicit `copy()` built-in for shallow cloning.

---

## 2. Object Representation (`Obj` Header)
Every heap-allocated value begins with a 16-byte intrusive `Obj` header (`include/value.h`):

```c
typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_INSTANCE,
    OBJ_COROUTINE,
} ObjType;

typedef struct Obj {
    ObjType type;
    uint8_t is_marked;
    struct Obj *next;
} Obj;
```

All allocated objects are linked into a single intrusive list (`gc_objects`) via `obj.next`.

### Heap Object Types
1. **`ObjString` (`VAL_STRING`)**:
   - Contains `Obj obj`, `int length`, `uint32_t hash` (FNV-1a), and a flexible array `char chars[1]`.
   - `Value.as.string` points directly to `ObjString.chars` for zero-cost C-string interoperability, while `AS_OBJ_STRING(v)` recovers the `ObjString*` header via `offsetof(ObjString, chars)`.
   - All strings are deduplicated in an open-addressed string intern table (`intern_table`). Pointer equality (`a == b`) suffices for equality of interned `ObjString*` keys.
2. **`ObjArray` / `ValueArray` (`VAL_ARRAY`)**:
   - Heap-allocated dynamic array (`Value *items`, `int count`, `int capacity`).
   - Passing an array to a function or assigning `var b is a` shares the underlying `ObjArray*` reference; mutating `b[0]` or calling `push(b, x)` is visible through `a`.
3. **`ObjMap` / `ValueMap` (`VAL_MAP`)**:
   - Hybrid open-addressed hash table (`int *buckets`, `int bucket_cap`) + dense `ValueMapEntry *entries` array preserving insertion order while providing $O(1)$ key lookup using interned `ObjString*` keys.
4. **`ObjInstance` (`VAL_STRUCT`)**:
   - Instance of a declared `struct`, holding `StructDef *struct_def`, `int field_count`, and `Value *fields`.
5. **`ObjCoroutine` (`VAL_COROUTINE`)**:
   - First-class coroutine state holding its own growable `Value *stack`, `CallFrame *call_stack`, `TryFrame *try_stack`, instruction pointer `ip`, and `CoroState` (`CORO_SUSPENDED`, `CORO_RUNNING`, `CORO_DEAD`).

### Reference vs. Copy Semantics
- Assignment (`set b to a` / `b = a`), argument passing, and struct/array/map storage copy the 16-byte `Value` descriptor, sharing the heap pointer for `VAL_ARRAY`, `VAL_MAP`, `VAL_STRUCT`, and `VAL_COROUTINE`.
- `copy(x)` creates a distinct heap allocation with shallow-copied elements/entries/fields.

---

## 3. Mark-Sweep Garbage Collector

### Root Sources
When `gc_collect()` runs, the **Mark Phase** traces from all live roots:
1. **Temporary HandleScope Roots (`gc_temp_roots`)**:
   - Every allocation constructor (`make_string_value`, `make_array_value`, `make_map_value`, `make_instance_value`, `make_coroutine_value`) automatically pushes the new `Value` onto `gc_temp_roots`.
   - Both `vm_execute()` (per bytecode instruction) and `exec_stmt()` (per AST statement) bracket execution with `gc_save_roots()` / `gc_restore_roots(saved)`. This prevents mid-expression allocations (such as key/value pairs in `OP_BUILD_MAP` or string concatenation in `OP_ADD`) from being swept before they are pushed onto the VM stack or bound in an `Environment`.
2. **Bytecode VM Roots (`registered_vms`)**:
   - Active value stack (`vm->stack[0 .. vm->sp - 1]`).
   - Saved main context stack (`vm->main_ctx.stack`) when executing inside a coroutine.
   - Active coroutine (`vm->current_coro`) and its `caller` chain.
   - Global variable slots (`vm->globals[0 .. vm->global_count - 1]`).
   - Compiled `Chunk` cached `ObjString*` constants and struct field default values.
3. **Tree-Walking Interpreter Roots (`registered_envs`)**:
   - All active lexical scopes (`env->scopes[0 .. env->count - 1]`).
4. **Registered `StructDef` Metadata (`registered_structs`)**:
   - Struct names, field names, method names, and default field values.

### Transitive Tracing
- `OBJ_ARRAY`: marks `items[0 .. count - 1]`.
- `OBJ_MAP`: marks each entry's `key_obj` and `value`.
- `OBJ_INSTANCE`: marks `struct_def` and `fields[0 .. field_count - 1]`.
- `OBJ_COROUTINE`: marks `stack[0 .. sp - 1]`, `init_args[0 .. init_arg_count - 1]`, and `caller`.

### Weak Intern Table Sweep & Sweep Phase
1. Before sweeping `gc_objects`, `gc_remove_white_strings()` scans the string intern table and tombstones any `ObjString*` whose `is_marked == 0`, so unreachable strings do not dangle in the intern table.
2. The sweep loop walks `gc_objects`:
   - Unmarked objects (`is_marked == 0`) are unlinked, their payload buffers are freed (`free_object`), and `gc_live_bytes` / `gc_objects_freed` counters are updated.
   - Marked objects have `is_marked` cleared to `0` for the next cycle.
3. `gc_next_threshold` adapts to `max(1 MB, gc_live_bytes * 2)`.

### Stress Testing (`--gc-stress`)
Passing `--gc-stress` sets `gc_stress_mode = 1`, forcing a full `gc_collect()` before every heap allocation (`allocate_obj`). This immediately exposes any missing root registration or unprotected C-stack temporary value.
