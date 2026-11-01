#ifndef VESBO_VALUE_H
#define VESBO_VALUE_H

#include <stddef.h>
#include <stdint.h>

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

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_ARRAY,
    VAL_MAP,
    VAL_STRUCT,
    VAL_COROUTINE,
    VAL_NONE,
} ValueType;

typedef struct Value Value;
typedef struct ObjString ObjString;
typedef struct ValueArray ValueArray;
typedef struct ValueArray ObjArray;
typedef struct ValueMapEntry ValueMapEntry;
typedef struct ValueMap ValueMap;
typedef struct ValueMap ObjMap;
typedef struct StructDef StructDef;
typedef struct ObjInstance ObjInstance;
typedef struct ObjCoroutine ObjCoroutine;

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[1];
};

struct Value {
    ValueType type;
    union {
        double number;
        char *string;            /* Points to ObjString->chars (interned) */
        int boolean;
        ValueArray *array;       /* Heap ObjArray (reference semantics) */
        ValueMap *map;           /* Heap ObjMap (reference semantics) */
        ObjInstance *instance;   /* Heap ObjInstance (reference semantics) */
        ObjCoroutine *coroutine; /* Heap ObjCoroutine */
        Obj *obj;
    } as;
};

struct ValueArray {
    Obj obj;
    Value *items;
    int count;
    int capacity;
};

struct ValueMapEntry {
    ObjString *key_obj;
    char *key; /* Points to key_obj->chars */
    Value value;
};

struct ValueMap {
    Obj obj;
    ValueMapEntry *entries;
    int count;
    int capacity;
    int *buckets;
    int bucket_cap;
};

typedef struct {
    ObjString *name;
    int func_idx;      /* Bytecode function index, or -1 */
    void *ast_stmt;    /* Stmt* for tree-walking interpreter */
} StructMethod;

struct StructDef {
    char *name;
    ObjString *name_obj;
    int field_count;
    ObjString **field_names;
    Value *field_defaults;
    void **field_default_exprs; /* Expr* array for interpreter */
    int method_count;
    int method_cap;
    StructMethod *methods;
};

struct ObjInstance {
    Obj obj;
    StructDef *struct_def;
    int field_count;
    Value *fields;
};

typedef enum {
    CORO_SUSPENDED,
    CORO_RUNNING,
    CORO_DEAD,
} CoroState;

/* Convert a VAL_STRING Value to its owning ObjString* header */
#define AS_OBJ_STRING(v) ((ObjString *)((char *)(v).as.string - offsetof(ObjString, chars)))

/* Value constructors */
Value make_number_value(double n);
Value make_string_value(const char *s);
Value make_string_value_len(const char *s, size_t len);
Value make_obj_string_value(ObjString *s);
Value make_bool_value(int b);
Value make_none_value(void);
Value make_array_value(void);
Value make_map_value(void);
Value make_instance_value(StructDef *def);
Value make_coroutine_value(int func_idx, const char *func_name, Value *args, int arg_count);

/* Explicit shallow copy built-in support for arrays, maps, and structs */
Value value_copy(Value v);

/* Interned string API */
uint32_t hash_string_bytes(const char *chars, size_t len);
ObjString *intern_string(const char *chars, size_t len);
Obj *value_as_obj(Value v);

/* Array operations */
void value_array_init(ValueArray *arr);
void value_array_push(ValueArray *arr, Value v);

/* Map operations (hashed with interned string keys) */
void value_map_init(ValueMap *map);
int value_map_set(ValueMap *map, const char *key, Value v);
int value_map_get(ValueMap *map, const char *key, Value *out);
int value_map_has(ValueMap *map, const char *key);
int value_map_remove(ValueMap *map, const char *key);

int value_map_set_obj(ValueMap *map, ObjString *key, Value v);
int value_map_get_obj(ValueMap *map, ObjString *key, Value *out);
int value_map_has_obj(ValueMap *map, ObjString *key);
int value_map_remove_obj(ValueMap *map, ObjString *key);

/* Struct definition & instance helpers */
StructDef *struct_def_new(const char *name, int field_count);
void struct_def_free(StructDef *def);
int struct_def_find_field(StructDef *def, ObjString *field_name);
int struct_def_find_field_str(StructDef *def, const char *field_name);
void struct_def_add_method(StructDef *def, const char *name, int func_idx, void *ast_stmt);
StructMethod *struct_def_find_method(StructDef *def, ObjString *name);
StructMethod *struct_def_find_method_str(StructDef *def, const char *name);

/* Value inspection and comparison */
int value_is_truthy(Value v);
int values_equal(Value a, Value b);
char *value_to_display_string(Value v);
const char *value_type_name(Value v);

/* Garbage Collector and Profiler API */
struct VM;
struct Environment;
struct Chunk;

void gc_collect(void);
void value_cleanup(void);

/* HandleScope / Temporary Root Protection */
void gc_push_root(Value v);
void gc_pop_roots(int count);
int gc_save_roots(void);
void gc_restore_roots(int saved_top);

/* Root source registration */
void gc_register_vm(struct VM *vm);
void gc_unregister_vm(struct VM *vm);
void gc_register_env(struct Environment *env);
void gc_unregister_env(struct Environment *env);
void gc_register_chunk(struct Chunk *chunk);
void gc_unregister_chunk(struct Chunk *chunk);
void gc_register_struct_def(StructDef *def);
void gc_clear_struct_defs(void);

/* Flags and Profiler Counters */
void gc_set_stress(int enabled);
int gc_get_stress(void);
void gc_set_profile(int enabled);
int gc_get_profile(void);

void gc_count_instruction(void);
uint64_t gc_get_instructions(void);
uint64_t gc_get_total_allocations(void);
uint64_t gc_get_total_bytes(void);
size_t gc_get_live_bytes(void);
uint64_t gc_get_collections(void);
uint64_t gc_get_objects_freed(void);
size_t gc_get_object_count(void);
void gc_reset_stats(void);
void gc_print_profile_report(void);

/* Stack trace reporting */
void vesbo_set_last_stack_trace(const char *trace);
const char *vesbo_get_last_stack_trace(void);

#endif