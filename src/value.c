#include "../include/value.h"
#include "../include/vm.h"
#include "../include/environment.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * GC Heap & Profiler State
 * ============================================================================ */

static Obj *gc_objects = NULL;
static size_t gc_object_count = 0;
static size_t gc_live_bytes = 0;
static size_t gc_next_threshold = 64 * 1024;
static int gc_stress_enabled = 0;
static int gc_profile_enabled = 0;
static int gc_in_progress = 0;

static uint64_t stat_instructions = 0;
static uint64_t stat_total_allocations = 0;
static uint64_t stat_total_bytes = 0;
static uint64_t stat_gc_collections = 0;
static uint64_t stat_gc_objects_freed = 0;

static char last_stack_trace_buf[4096] = {0};

void vesbo_set_last_stack_trace(const char *trace) {
    if (trace) {
        strncpy(last_stack_trace_buf, trace, sizeof(last_stack_trace_buf) - 1);
        last_stack_trace_buf[sizeof(last_stack_trace_buf) - 1] = '\0';
    } else {
        last_stack_trace_buf[0] = '\0';
    }
}

const char *vesbo_get_last_stack_trace(void) {
    return last_stack_trace_buf;
}

/* Temporary root stack (HandleScope) */
static Value *gc_temp_roots = NULL;
static int gc_temp_count = 0;
static int gc_temp_cap = 0;

/* Registered VM, Environment, and Chunk root sources */
static struct VM **registered_vms = NULL;
static int registered_vm_count = 0;
static int registered_vm_cap = 0;

static struct Environment **registered_envs = NULL;
static int registered_env_count = 0;
static int registered_env_cap = 0;

static struct Chunk **registered_chunks = NULL;
static int registered_chunk_count = 0;
static int registered_chunk_cap = 0;

static StructDef **registered_structs = NULL;
static int registered_struct_count = 0;
static int registered_struct_cap = 0;

/* Interned String Hash Table (Open Addressing) */
static ObjString **intern_entries = NULL;
static int intern_count = 0;
static int intern_tombstones = 0;
static int intern_cap = 0;

static char *copy_cstr(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

uint32_t hash_string_bytes(const char *chars, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619u;
    }
    return hash;
}

/* ============================================================================
 * Root Registration & HandleScope
 * ============================================================================ */

void gc_push_root(Value v) {
    if (v.type != VAL_STRING && v.type != VAL_ARRAY &&
        v.type != VAL_MAP && v.type != VAL_STRUCT &&
        v.type != VAL_COROUTINE) {
        return;
    }
    if (gc_temp_count >= gc_temp_cap) {
        gc_temp_cap = gc_temp_cap == 0 ? 256 : gc_temp_cap * 2;
        gc_temp_roots = realloc(gc_temp_roots, sizeof(Value) * gc_temp_cap);
    }
    gc_temp_roots[gc_temp_count++] = v;
}

void gc_pop_roots(int count) {
    if (count <= 0) return;
    gc_temp_count -= count;
    if (gc_temp_count < 0) gc_temp_count = 0;
}

int gc_save_roots(void) {
    return gc_temp_count;
}

void gc_restore_roots(int saved_top) {
    if (saved_top >= 0 && saved_top <= gc_temp_count) {
        gc_temp_count = saved_top;
    }
}

void gc_register_vm(struct VM *vm) {
    if (!vm) return;
    for (int i = 0; i < registered_vm_count; i++) {
        if (registered_vms[i] == vm) return;
    }
    if (registered_vm_count >= registered_vm_cap) {
        registered_vm_cap = registered_vm_cap == 0 ? 16 : registered_vm_cap * 2;
        registered_vms = realloc(registered_vms, sizeof(struct VM *) * (size_t)registered_vm_cap);
    }
    registered_vms[registered_vm_count++] = vm;
}

void gc_unregister_vm(struct VM *vm) {
    for (int i = 0; i < registered_vm_count; i++) {
        if (registered_vms[i] == vm) {
            for (int j = i; j < registered_vm_count - 1; j++) {
                registered_vms[j] = registered_vms[j + 1];
            }
            registered_vm_count--;
            return;
        }
    }
}

void gc_register_env(struct Environment *env) {
    if (!env) return;
    for (int i = 0; i < registered_env_count; i++) {
        if (registered_envs[i] == env) return;
    }
    if (registered_env_count >= registered_env_cap) {
        registered_env_cap = registered_env_cap == 0 ? 16 : registered_env_cap * 2;
        registered_envs = realloc(registered_envs, sizeof(struct Environment *) * (size_t)registered_env_cap);
    }
    registered_envs[registered_env_count++] = env;
}

void gc_unregister_env(struct Environment *env) {
    for (int i = 0; i < registered_env_count; i++) {
        if (registered_envs[i] == env) {
            for (int j = i; j < registered_env_count - 1; j++) {
                registered_envs[j] = registered_envs[j + 1];
            }
            registered_env_count--;
            return;
        }
    }
}

void gc_register_chunk(struct Chunk *chunk) {
    if (!chunk) return;
    for (int i = 0; i < registered_chunk_count; i++) {
        if (registered_chunks[i] == chunk) return;
    }
    if (registered_chunk_count >= registered_chunk_cap) {
        registered_chunk_cap = registered_chunk_cap == 0 ? 16 : registered_chunk_cap * 2;
        registered_chunks = realloc(registered_chunks, sizeof(struct Chunk *) * (size_t)registered_chunk_cap);
    }
    registered_chunks[registered_chunk_count++] = chunk;
}

void gc_unregister_chunk(struct Chunk *chunk) {
    for (int i = 0; i < registered_chunk_count; i++) {
        if (registered_chunks[i] == chunk) {
            for (int j = i; j < registered_chunk_count - 1; j++) {
                registered_chunks[j] = registered_chunks[j + 1];
            }
            registered_chunk_count--;
            return;
        }
    }
}

void gc_register_struct_def(StructDef *def) {
    if (!def) return;
    for (int i = 0; i < registered_struct_count; i++) {
        if (registered_structs[i] == def) return;
    }
    if (registered_struct_count >= registered_struct_cap) {
        registered_struct_cap = registered_struct_cap == 0 ? 16 : registered_struct_cap * 2;
        registered_structs = realloc(registered_structs, sizeof(StructDef *) * registered_struct_cap);
    }
    registered_structs[registered_struct_count++] = def;
}

void gc_clear_struct_defs(void) {
    for (int i = 0; i < registered_struct_count; i++) {
        struct_def_free(registered_structs[i]);
    }
    free(registered_structs);
    registered_structs = NULL;
    registered_struct_count = 0;
    registered_struct_cap = 0;
}

/* ============================================================================
 * Profiler Counters & Flags
 * ============================================================================ */

void gc_set_stress(int enabled) { gc_stress_enabled = enabled ? 1 : 0; }
int gc_get_stress(void) { return gc_stress_enabled; }
void gc_set_profile(int enabled) { gc_profile_enabled = enabled ? 1 : 0; }
int gc_get_profile(void) { return gc_profile_enabled; }

void gc_count_instruction(void) { stat_instructions++; }
uint64_t gc_get_instructions(void) { return stat_instructions; }
uint64_t gc_get_total_allocations(void) { return stat_total_allocations; }
uint64_t gc_get_total_bytes(void) { return stat_total_bytes; }
size_t gc_get_live_bytes(void) { return gc_live_bytes; }
uint64_t gc_get_collections(void) { return stat_gc_collections; }
uint64_t gc_get_objects_freed(void) { return stat_gc_objects_freed; }
size_t gc_get_object_count(void) { return gc_object_count; }

void gc_reset_stats(void) {
    stat_instructions = 0;
    stat_total_allocations = 0;
    stat_total_bytes = 0;
    stat_gc_collections = 0;
    stat_gc_objects_freed = 0;
}

void gc_print_profile_report(void) {
    fprintf(stderr,
            "[Profile] Instructions: %llu | Allocations: %llu (%llu bytes) | GC runs: %llu | Freed objects: %llu | Live bytes: %zu\n",
            (unsigned long long)stat_instructions,
            (unsigned long long)stat_total_allocations,
            (unsigned long long)stat_total_bytes,
            (unsigned long long)stat_gc_collections,
            (unsigned long long)stat_gc_objects_freed,
            gc_live_bytes);
}

/* ============================================================================
 * Mark-Sweep Garbage Collector
 * ============================================================================ */

Obj *value_as_obj(Value v) {
    switch (v.type) {
        case VAL_STRING:
            return v.as.string ? &AS_OBJ_STRING(v)->obj : NULL;
        case VAL_ARRAY:
            return v.as.array ? &v.as.array->obj : NULL;
        case VAL_MAP:
            return v.as.map ? &v.as.map->obj : NULL;
        case VAL_STRUCT:
            return v.as.instance ? &v.as.instance->obj : NULL;
        case VAL_COROUTINE:
            return v.as.coroutine ? &v.as.coroutine->obj : NULL;
        default:
            return NULL;
    }
}

static void gc_mark_obj(Obj *obj);

static void gc_mark_value(Value v) {
    Obj *obj = value_as_obj(v);
    if (obj) gc_mark_obj(obj);
}

static void gc_mark_struct_def(StructDef *def) {
    if (!def) return;
    if (def->name_obj) gc_mark_obj(&def->name_obj->obj);
    for (int i = 0; i < def->field_count; i++) {
        if (def->field_names && def->field_names[i]) {
            gc_mark_obj(&def->field_names[i]->obj);
        }
        if (def->field_defaults) {
            gc_mark_value(def->field_defaults[i]);
        }
    }
    for (int i = 0; i < def->method_count; i++) {
        if (def->methods[i].name) {
            gc_mark_obj(&def->methods[i].name->obj);
        }
    }
}

static void gc_mark_obj(Obj *obj) {
    if (!obj || obj->is_marked) return;
    obj->is_marked = 1;

    switch (obj->type) {
        case OBJ_STRING:
            break;

        case OBJ_ARRAY: {
            ValueArray *arr = (ValueArray *)obj;
            for (int i = 0; i < arr->count; i++) {
                gc_mark_value(arr->items[i]);
            }
            break;
        }

        case OBJ_MAP: {
            ValueMap *map = (ValueMap *)obj;
            for (int i = 0; i < map->count; i++) {
                if (map->entries[i].key_obj) {
                    gc_mark_obj(&map->entries[i].key_obj->obj);
                }
                gc_mark_value(map->entries[i].value);
            }
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance *inst = (ObjInstance *)obj;
            gc_mark_struct_def(inst->struct_def);
            for (int i = 0; i < inst->field_count; i++) {
                gc_mark_value(inst->fields[i]);
            }
            break;
        }

        case OBJ_COROUTINE: {
            ObjCoroutine *co = (ObjCoroutine *)obj;
            for (int i = 0; i < co->sp; i++) {
                gc_mark_value(co->stack[i]);
            }
            for (int i = 0; i < co->init_arg_count; i++) {
                gc_mark_value(co->init_args[i]);
            }
            if (co->caller) {
                gc_mark_obj(&co->caller->obj);
            }
            break;
        }
    }
}

static size_t obj_byte_size(Obj *obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString *s = (ObjString *)obj;
            return offsetof(ObjString, chars) + (size_t)s->length + 1;
        }
        case OBJ_ARRAY: {
            ValueArray *arr = (ValueArray *)obj;
            return sizeof(ValueArray) + sizeof(Value) * (size_t)arr->capacity;
        }
        case OBJ_MAP: {
            ValueMap *map = (ValueMap *)obj;
            return sizeof(ValueMap) +
                   sizeof(ValueMapEntry) * (size_t)map->capacity +
                   sizeof(int) * (size_t)map->bucket_cap;
        }
        case OBJ_INSTANCE: {
            ObjInstance *inst = (ObjInstance *)obj;
            return sizeof(ObjInstance) + sizeof(Value) * (size_t)inst->field_count;
        }
        case OBJ_COROUTINE: {
            ObjCoroutine *co = (ObjCoroutine *)obj;
            return sizeof(ObjCoroutine) +
                   sizeof(Value) * (size_t)co->stack_cap +
                   sizeof(CallFrame) * (size_t)co->call_cap +
                   sizeof(TryFrame) * (size_t)co->try_cap;
        }
    }
    return sizeof(Obj);
}

static void free_obj(Obj *obj) {
    size_t sz = obj_byte_size(obj);
    if (gc_live_bytes >= sz) gc_live_bytes -= sz;
    else gc_live_bytes = 0;

    switch (obj->type) {
        case OBJ_STRING:
            free(obj);
            break;
        case OBJ_ARRAY: {
            ValueArray *arr = (ValueArray *)obj;
            free(arr->items);
            free(arr);
            break;
        }
        case OBJ_MAP: {
            ValueMap *map = (ValueMap *)obj;
            free(map->entries);
            free(map->buckets);
            free(map);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *inst = (ObjInstance *)obj;
            free(inst->fields);
            free(inst);
            break;
        }
        case OBJ_COROUTINE: {
            ObjCoroutine *co = (ObjCoroutine *)obj;
            free(co->func_name);
            free(co->stack);
            free(co->call_stack);
            free(co->try_stack);
            free(co->init_args);
            free(co);
            break;
        }
    }
}

static void gc_remove_white_strings(void) {
    if (!intern_entries) return;
    for (int i = 0; i < intern_cap; i++) {
        ObjString *entry = intern_entries[i];
        if (entry != NULL && entry != (ObjString *)(uintptr_t)1 && !entry->obj.is_marked) {
            /* Replace with tombstone (1) so probe chains remain valid */
            intern_entries[i] = (ObjString *)(uintptr_t)1;
            intern_count--;
            intern_tombstones++;
        }
    }
}

static void gc_mark_chunk(struct Chunk *chunk) {
    if (!chunk) return;
    if (chunk->string_objs) {
        for (size_t i = 0; i < chunk->strings_len; i++) {
            if (chunk->string_objs[i]) {
                gc_mark_obj(&chunk->string_objs[i]->obj);
            }
        }
    }
    if (chunk->structs) {
        for (size_t i = 0; i < chunk->structs_len; i++) {
            ChunkStructDef *cs = &chunk->structs[i];
            for (int f = 0; f < cs->field_count; f++) {
                gc_mark_value(cs->field_defaults[f]);
            }
            if (cs->runtime_def) {
                gc_mark_struct_def(cs->runtime_def);
            }
        }
    }
}

void gc_collect(void) {
    if (gc_in_progress) return;
    gc_in_progress = 1;
    stat_gc_collections++;

    /* 1. Mark roots from temporary HandleScope */
    for (int i = 0; i < gc_temp_count; i++) {
        gc_mark_value(gc_temp_roots[i]);
    }

    /* 2. Mark roots from active VMs */
    for (int v = 0; v < registered_vm_count; v++) {
        struct VM *vm = registered_vms[v];
        if (!vm) continue;
        for (int i = 0; i < vm->sp; i++) {
            gc_mark_value(vm->stack[i]);
        }
        for (int i = 0; i < vm->global_count; i++) {
            if (vm->global_defined && vm->global_defined[i]) {
                gc_mark_value(vm->globals[i]);
            }
        }
        if (vm->current_coro) {
            gc_mark_obj(&vm->current_coro->obj);
        }
        for (int i = 0; i < vm->pinned_coro_count; i++) {
            if (vm->pinned_coros[i] && vm->pinned_coros[i]->state != CORO_DEAD) {
                gc_mark_obj(&vm->pinned_coros[i]->obj);
            }
        }
        if (vm->has_saved_main && vm->main_ctx.stack) {
            for (int i = 0; i < vm->main_ctx.sp; i++) {
                gc_mark_value(vm->main_ctx.stack[i]);
            }
        }
        gc_mark_chunk(vm->chunk);
    }

    /* 2b. Mark roots from registered Chunks (e.g. during compilation) */
    for (int c = 0; c < registered_chunk_count; c++) {
        gc_mark_chunk(registered_chunks[c]);
    }

    /* 3. Mark roots from active Interpreter Environments */
    for (int e = 0; e < registered_env_count; e++) {
        struct Environment *env = registered_envs[e];
        if (!env) continue;
        for (int s = 0; s < env->count; s++) {
            Scope *scope = &env->scopes[s];
            for (int i = 0; i < scope->count; i++) {
                gc_mark_value(scope->values[i]);
            }
        }
    }

    /* 4. Mark registered StructDefs */
    for (int i = 0; i < registered_struct_count; i++) {
        gc_mark_struct_def(registered_structs[i]);
    }

    /* 5. Remove unreachable interned strings before sweep */
    gc_remove_white_strings();

    /* 6. Sweep phase */
    Obj **obj_ptr = &gc_objects;
    while (*obj_ptr != NULL) {
        Obj *curr = *obj_ptr;
        if (!curr->is_marked) {
            *obj_ptr = curr->next;
            gc_object_count--;
            stat_gc_objects_freed++;
            free_obj(curr);
        } else {
            curr->is_marked = 0;
            obj_ptr = &curr->next;
        }
    }

    gc_next_threshold = gc_live_bytes * 2;
    if (gc_next_threshold < 64 * 1024) {
        gc_next_threshold = 64 * 1024;
    }

    gc_in_progress = 0;
}

static Obj *allocate_obj(size_t size, ObjType type) {
    if (gc_stress_enabled || gc_live_bytes + size > gc_next_threshold) {
        gc_collect();
    }

    Obj *obj = malloc(size);
    if (!obj) return NULL;
    obj->type = type;
    obj->is_marked = 0;
    obj->next = gc_objects;
    gc_objects = obj;

    gc_object_count++;
    gc_live_bytes += size;
    stat_total_allocations++;
    stat_total_bytes += size;

    return obj;
}

/* ============================================================================
 * Interned Strings
 * ============================================================================ */

static void intern_table_grow(void) {
    int new_cap;
    if (intern_cap == 0) {
        new_cap = 128;
    } else if ((intern_count + 1) * 4 >= intern_cap * 3) {
        new_cap = intern_cap * 2;
    } else {
        new_cap = intern_cap; /* Rehash at same capacity to purge tombstones */
    }
    ObjString **new_entries = calloc((size_t)new_cap, sizeof(ObjString *));
    intern_count = 0;
    intern_tombstones = 0;

    for (int i = 0; i < intern_cap; i++) {
        ObjString *entry = intern_entries[i];
        if (!entry || entry == (ObjString *)(uintptr_t)1) continue;

        uint32_t idx = entry->hash & (uint32_t)(new_cap - 1);
        while (new_entries[idx] != NULL) {
            idx = (idx + 1) & (uint32_t)(new_cap - 1);
        }
        new_entries[idx] = entry;
        intern_count++;
    }

    free(intern_entries);
    intern_entries = new_entries;
    intern_cap = new_cap;
}

ObjString *intern_string(const char *chars, size_t len) {
    if (!chars) {
        chars = "";
        len = 0;
    }
    uint32_t hash = hash_string_bytes(chars, len);

    if (intern_cap > 0) {
        uint32_t idx = hash & (uint32_t)(intern_cap - 1);
        uint32_t start_idx = idx;
        do {
            ObjString *entry = intern_entries[idx];
            if (entry == NULL) break;
            if (entry != (ObjString *)(uintptr_t)1 &&
                entry->length == (int)len &&
                entry->hash == hash &&
                memcmp(entry->chars, chars, len) == 0) {
                return entry;
            }
            idx = (idx + 1) & (uint32_t)(intern_cap - 1);
        } while (idx != start_idx);
    }

    /* Copy chars to a scratch buffer in case chars points inside an ObjString
     * that is not rooted when allocate_obj runs under --gc-stress */
    char *scratch = NULL;
    if (len > 0) {
        scratch = malloc(len);
        if (scratch) memcpy(scratch, chars, len);
    }

    /* Allocate new ObjString (note: allocate_obj may run GC and mutate intern_entries) */
    size_t alloc_sz = offsetof(ObjString, chars) + len + 1;
    ObjString *s = (ObjString *)allocate_obj(alloc_sz, OBJ_STRING);
    s->length = (int)len;
    s->hash = hash;
    if (scratch) {
        memcpy(s->chars, scratch, len);
        free(scratch);
    }
    s->chars[len] = '\0';

    if ((intern_count + intern_tombstones + 1) * 4 >= intern_cap * 3) {
        intern_table_grow();
    }

    uint32_t idx = hash & (uint32_t)(intern_cap - 1);
    int tombstone_idx = -1;
    while (intern_entries[idx] != NULL) {
        if (intern_entries[idx] == (ObjString *)(uintptr_t)1) {
            if (tombstone_idx < 0) tombstone_idx = (int)idx;
        }
        idx = (idx + 1) & (uint32_t)(intern_cap - 1);
    }

    if (tombstone_idx >= 0) {
        intern_entries[tombstone_idx] = s;
        intern_tombstones--;
        intern_count++;
    } else {
        intern_entries[idx] = s;
        intern_count++;
    }

    return s;
}

/* ============================================================================
 * Value Constructors
 * ============================================================================ */

Value make_number_value(double n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

Value make_obj_string_value(ObjString *s) {
    Value v;
    v.type = VAL_STRING;
    v.as.string = s ? s->chars : intern_string("", 0)->chars;
    gc_push_root(v);
    return v;
}

Value make_string_value_len(const char *s, size_t len) {
    ObjString *obj = intern_string(s, len);
    return make_obj_string_value(obj);
}

Value make_string_value(const char *s) {
    if (!s) s = "";
    return make_string_value_len(s, strlen(s));
}

Value make_bool_value(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b ? 1 : 0;
    return v;
}

Value make_none_value(void) {
    Value v;
    v.type = VAL_NONE;
    v.as.obj = NULL;
    return v;
}

Value make_array_value(void) {
    ValueArray *arr = (ValueArray *)allocate_obj(sizeof(ValueArray), OBJ_ARRAY);
    value_array_init(arr);
    Value v;
    v.type = VAL_ARRAY;
    v.as.array = arr;
    gc_push_root(v);
    return v;
}

Value make_map_value(void) {
    ValueMap *map = (ValueMap *)allocate_obj(sizeof(ValueMap), OBJ_MAP);
    value_map_init(map);
    Value v;
    v.type = VAL_MAP;
    v.as.map = map;
    gc_push_root(v);
    return v;
}

Value make_instance_value(StructDef *def) {
    int count = def ? def->field_count : 0;
    ObjInstance *inst = (ObjInstance *)allocate_obj(sizeof(ObjInstance), OBJ_INSTANCE);
    inst->struct_def = def;
    inst->field_count = count;
    inst->fields = count > 0 ? malloc(sizeof(Value) * (size_t)count) : NULL;
    if (count > 0) {
        gc_live_bytes += sizeof(Value) * (size_t)count;
        stat_total_bytes += sizeof(Value) * (size_t)count;
        for (int i = 0; i < count; i++) {
            inst->fields[i] = (def && def->field_defaults) ? def->field_defaults[i] : make_none_value();
        }
    }
    Value v;
    v.type = VAL_STRUCT;
    v.as.instance = inst;
    gc_push_root(v);
    return v;
}

Value make_coroutine_value(int func_idx, const char *func_name, Value *args, int arg_count) {
    if (arg_count > 0 && args) {
        for (int i = 0; i < arg_count; i++) {
            gc_push_root(args[i]);
        }
    }
    ObjCoroutine *co = (ObjCoroutine *)allocate_obj(sizeof(ObjCoroutine), OBJ_COROUTINE);
    co->state = CORO_SUSPENDED;
    co->started = 0;
    co->func_idx = func_idx;
    co->func_name = copy_cstr(func_name ? func_name : "");
    co->ip = NULL;
    co->stack_cap = 64;
    co->stack = malloc(sizeof(Value) * (size_t)co->stack_cap);
    co->sp = 0;
    co->call_cap = 16;
    co->call_stack = malloc(sizeof(CallFrame) * (size_t)co->call_cap);
    co->call_sp = 0;
    co->try_cap = 8;
    co->try_stack = malloc(sizeof(TryFrame) * (size_t)co->try_cap);
    co->try_sp = 0;
    co->caller = NULL;

    size_t extra_bytes = sizeof(Value) * (size_t)co->stack_cap +
                         sizeof(CallFrame) * (size_t)co->call_cap +
                         sizeof(TryFrame) * (size_t)co->try_cap;
    gc_live_bytes += extra_bytes;
    stat_total_bytes += extra_bytes;

    if (arg_count > 0 && args) {
        co->init_args = malloc(sizeof(Value) * (size_t)arg_count);
        memcpy(co->init_args, args, sizeof(Value) * (size_t)arg_count);
        co->init_arg_count = arg_count;
    } else {
        co->init_args = NULL;
        co->init_arg_count = 0;
    }

    Value v;
    v.type = VAL_COROUTINE;
    v.as.coroutine = co;
    gc_push_root(v);
    return v;
}

/* ============================================================================
 * Array & Hashed Map Operations
 * ============================================================================ */

void value_array_init(ValueArray *arr) {
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void value_array_push(ValueArray *arr, Value v) {
    if (!arr) return;
    if (arr->count >= arr->capacity) {
        int old_cap = arr->capacity;
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->items = realloc(arr->items, sizeof(Value) * (size_t)arr->capacity);
        size_t diff = sizeof(Value) * (size_t)(arr->capacity - old_cap);
        gc_live_bytes += diff;
        stat_total_bytes += diff;
    }
    arr->items[arr->count++] = v;
}

void value_map_init(ValueMap *map) {
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
    map->buckets = NULL;
    map->bucket_cap = 0;
}

static void value_map_rebuild_buckets(ValueMap *map, int new_bucket_cap) {
    int old_cap = map->bucket_cap;
    free(map->buckets);
    map->bucket_cap = new_bucket_cap;
    map->buckets = malloc(sizeof(int) * (size_t)new_bucket_cap);
    for (int i = 0; i < new_bucket_cap; i++) {
        map->buckets[i] = -1;
    }
    if (new_bucket_cap > old_cap) {
        size_t diff = sizeof(int) * (size_t)(new_bucket_cap - old_cap);
        gc_live_bytes += diff;
        stat_total_bytes += diff;
    }
    for (int i = 0; i < map->count; i++) {
        ObjString *k = map->entries[i].key_obj;
        uint32_t b = k->hash & (uint32_t)(new_bucket_cap - 1);
        while (map->buckets[b] >= 0) {
            b = (b + 1) & (uint32_t)(new_bucket_cap - 1);
        }
        map->buckets[b] = i;
    }
}

int value_map_set_obj(ValueMap *map, ObjString *key, Value v) {
    if (!map || !key) return 0;

    if (map->bucket_cap == 0 || (map->count + 1) * 4 >= map->bucket_cap * 3) {
        int new_bcap = map->bucket_cap == 0 ? 16 : map->bucket_cap * 2;
        value_map_rebuild_buckets(map, new_bcap);
    }

    uint32_t mask = (uint32_t)(map->bucket_cap - 1);
    uint32_t b = key->hash & mask;
    int insert_b = -1;

    while (map->buckets[b] != -1) {
        int entry_idx = map->buckets[b];
        if (entry_idx >= 0) {
            if (map->entries[entry_idx].key_obj == key) {
                map->entries[entry_idx].value = v;
                return 1;
            }
        } else if (insert_b < 0) {
            insert_b = (int)b; /* Tombstone (-2) */
        }
        b = (b + 1) & mask;
    }

    if (insert_b < 0) insert_b = (int)b;

    if (map->count >= map->capacity) {
        int old_cap = map->capacity;
        map->capacity = map->capacity == 0 ? 8 : map->capacity * 2;
        map->entries = realloc(map->entries, sizeof(ValueMapEntry) * (size_t)map->capacity);
        size_t diff = sizeof(ValueMapEntry) * (size_t)(map->capacity - old_cap);
        gc_live_bytes += diff;
        stat_total_bytes += diff;
    }

    int idx = map->count++;
    map->entries[idx].key_obj = key;
    map->entries[idx].key = key->chars;
    map->entries[idx].value = v;
    map->buckets[insert_b] = idx;
    return 1;
}

int value_map_get_obj(ValueMap *map, ObjString *key, Value *out) {
    if (!map || !key || map->bucket_cap == 0 || map->count == 0) return 0;
    uint32_t mask = (uint32_t)(map->bucket_cap - 1);
    uint32_t b = key->hash & mask;
    uint32_t start = b;
    do {
        int entry_idx = map->buckets[b];
        if (entry_idx == -1) return 0;
        if (entry_idx >= 0 && map->entries[entry_idx].key_obj == key) {
            if (out) *out = map->entries[entry_idx].value;
            return 1;
        }
        b = (b + 1) & mask;
    } while (b != start);
    return 0;
}

int value_map_has_obj(ValueMap *map, ObjString *key) {
    return value_map_get_obj(map, key, NULL);
}

int value_map_remove_obj(ValueMap *map, ObjString *key) {
    if (!map || !key || map->bucket_cap == 0 || map->count == 0) return 0;
    uint32_t mask = (uint32_t)(map->bucket_cap - 1);
    uint32_t b = key->hash & mask;
    uint32_t start = b;
    do {
        int entry_idx = map->buckets[b];
        if (entry_idx == -1) return 0;
        if (entry_idx >= 0 && map->entries[entry_idx].key_obj == key) {
            for (int j = entry_idx; j < map->count - 1; j++) {
                map->entries[j] = map->entries[j + 1];
            }
            map->count--;
            value_map_rebuild_buckets(map, map->bucket_cap);
            return 1;
        }
        b = (b + 1) & mask;
    } while (b != start);
    return 0;
}

int value_map_set(ValueMap *map, const char *key, Value v) {
    if (!map || !key) return 0;
    int saved = gc_save_roots();
    gc_push_root(v);
    ObjString *kobj = intern_string(key, strlen(key));
    int rc = value_map_set_obj(map, kobj, v);
    gc_restore_roots(saved);
    return rc;
}

int value_map_get(ValueMap *map, const char *key, Value *out) {
    if (!map || !key || map->count == 0) return 0;
    ObjString *kobj = intern_string(key, strlen(key));
    return value_map_get_obj(map, kobj, out);
}

int value_map_has(ValueMap *map, const char *key) {
    return value_map_get(map, key, NULL);
}

int value_map_remove(ValueMap *map, const char *key) {
    if (!map || !key || map->count == 0) return 0;
    ObjString *kobj = intern_string(key, strlen(key));
    return value_map_remove_obj(map, kobj);
}

/* ============================================================================
 * Struct Definitions
 * ============================================================================ */

StructDef *struct_def_new(const char *name, int field_count) {
    StructDef *def = calloc(1, sizeof(StructDef));
    if (!def) return NULL;
    def->name = copy_cstr(name ? name : "");
    def->field_count = field_count;
    def->field_names = field_count > 0 ? calloc((size_t)field_count, sizeof(ObjString *)) : NULL;
    def->field_defaults = field_count > 0 ? calloc((size_t)field_count, sizeof(Value)) : NULL;
    def->field_default_exprs = field_count > 0 ? calloc((size_t)field_count, sizeof(void *)) : NULL;
    for (int i = 0; i < field_count; i++) {
        def->field_defaults[i] = make_none_value();
    }
    def->method_count = 0;
    def->method_cap = 0;
    def->methods = NULL;
    gc_register_struct_def(def);
    def->name_obj = intern_string(def->name, strlen(def->name));
    return def;
}

void struct_def_free(StructDef *def) {
    if (!def) return;
    free(def->name);
    free(def->field_names);
    free(def->field_defaults);
    free(def->field_default_exprs);
    free(def->methods);
    free(def);
}

int struct_def_find_field(StructDef *def, ObjString *field_name) {
    if (!def || !field_name) return -1;
    for (int i = 0; i < def->field_count; i++) {
        if (def->field_names[i] == field_name) return i;
    }
    return -1;
}

int struct_def_find_field_str(StructDef *def, const char *field_name) {
    if (!def || !field_name) return -1;
    ObjString *fobj = intern_string(field_name, strlen(field_name));
    return struct_def_find_field(def, fobj);
}

void struct_def_add_method(StructDef *def, const char *name, int func_idx, void *ast_stmt) {
    if (!def || !name) return;
    ObjString *nobj = intern_string(name, strlen(name));
    for (int i = 0; i < def->method_count; i++) {
        if (def->methods[i].name == nobj) {
            if (func_idx >= 0) def->methods[i].func_idx = func_idx;
            if (ast_stmt != NULL) def->methods[i].ast_stmt = ast_stmt;
            return;
        }
    }
    if (def->method_count >= def->method_cap) {
        def->method_cap = def->method_cap == 0 ? 4 : def->method_cap * 2;
        def->methods = realloc(def->methods, sizeof(StructMethod) * (size_t)def->method_cap);
    }
    def->methods[def->method_count].name = nobj;
    def->methods[def->method_count].func_idx = func_idx;
    def->methods[def->method_count].ast_stmt = ast_stmt;
    def->method_count++;
}

StructMethod *struct_def_find_method(StructDef *def, ObjString *name) {
    if (!def || !name) return NULL;
    for (int i = 0; i < def->method_count; i++) {
        if (def->methods[i].name == name) return &def->methods[i];
    }
    return NULL;
}

StructMethod *struct_def_find_method_str(StructDef *def, const char *name) {
    if (!def || !name) return NULL;
    ObjString *nobj = intern_string(name, strlen(name));
    return struct_def_find_method(def, nobj);
}

/* ============================================================================
 * Shallow Copy (copy() built-in)
 * ============================================================================ */

Value value_copy(Value v) {
    gc_push_root(v);
    switch (v.type) {
        case VAL_ARRAY: {
            Value out = make_array_value();
            if (v.as.array) {
                for (int i = 0; i < v.as.array->count; i++) {
                    value_array_push(out.as.array, v.as.array->items[i]);
                }
            }
            return out;
        }
        case VAL_MAP: {
            Value out = make_map_value();
            if (v.as.map) {
                for (int i = 0; i < v.as.map->count; i++) {
                    value_map_set_obj(out.as.map, v.as.map->entries[i].key_obj, v.as.map->entries[i].value);
                }
            }
            return out;
        }
        case VAL_STRUCT: {
            if (!v.as.instance) return make_none_value();
            Value out = make_instance_value(v.as.instance->struct_def);
            for (int i = 0; i < v.as.instance->field_count; i++) {
                out.as.instance->fields[i] = v.as.instance->fields[i];
            }
            return out;
        }
        default:
            /* Numbers, immutable strings, booleans, none, coroutines */
            return v;
    }
}

/* ============================================================================
 * Truthiness, Equality, Type Name, and Display String
 * ============================================================================ */

int value_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:      return v.as.boolean;
        case VAL_NONE:      return 0;
        case VAL_NUMBER:    return v.as.number != 0;
        case VAL_STRING:    return v.as.string && v.as.string[0] != '\0';
        case VAL_ARRAY:     return v.as.array && v.as.array->count > 0;
        case VAL_MAP:       return v.as.map && v.as.map->count > 0;
        case VAL_STRUCT:    return v.as.instance != NULL;
        case VAL_COROUTINE: return v.as.coroutine != NULL && v.as.coroutine->state != CORO_DEAD;
    }
    return 0;
}

int values_equal(Value a, Value b) {
    if (a.type != b.type) {
        return 0;
    }
    switch (a.type) {
        case VAL_NUMBER: return a.as.number == b.as.number;
        case VAL_STRING:
            /* Interned strings: pointer equality is exact, fallback to strcmp for safety */
            return a.as.string == b.as.string || strcmp(a.as.string, b.as.string) == 0;
        case VAL_BOOL:   return a.as.boolean == b.as.boolean;
        case VAL_NONE:   return 1;
        case VAL_ARRAY:
            if (a.as.array == b.as.array) return 1;
            if (!a.as.array || !b.as.array) return 0;
            if (a.as.array->count != b.as.array->count) return 0;
            for (int i = 0; i < a.as.array->count; i++) {
                if (!values_equal(a.as.array->items[i], b.as.array->items[i])) return 0;
            }
            return 1;
        case VAL_MAP: {
            if (a.as.map == b.as.map) return 1;
            if (!a.as.map || !b.as.map) return 0;
            if (a.as.map->count != b.as.map->count) return 0;
            for (int i = 0; i < a.as.map->count; i++) {
                Value b_val;
                if (!value_map_get_obj(b.as.map, a.as.map->entries[i].key_obj, &b_val)) return 0;
                if (!values_equal(a.as.map->entries[i].value, b_val)) return 0;
            }
            return 1;
        }
        case VAL_STRUCT: {
            if (a.as.instance == b.as.instance) return 1;
            if (!a.as.instance || !b.as.instance) return 0;
            if (a.as.instance->struct_def != b.as.instance->struct_def) {
                if (!a.as.instance->struct_def || !b.as.instance->struct_def) return 0;
                if (strcmp(a.as.instance->struct_def->name, b.as.instance->struct_def->name) != 0) return 0;
            }
            if (a.as.instance->field_count != b.as.instance->field_count) return 0;
            for (int i = 0; i < a.as.instance->field_count; i++) {
                if (!values_equal(a.as.instance->fields[i], b.as.instance->fields[i])) return 0;
            }
            return 1;
        }
        case VAL_COROUTINE:
            return a.as.coroutine == b.as.coroutine;
    }
    return 0;
}

const char *value_type_name(Value v) {
    switch (v.type) {
        case VAL_NUMBER:    return "number";
        case VAL_STRING:    return "string";
        case VAL_BOOL:      return "boolean";
        case VAL_ARRAY:     return "array";
        case VAL_MAP:       return "map";
        case VAL_STRUCT:    return "struct";
        case VAL_COROUTINE: return "coroutine";
        case VAL_NONE:      return "none";
    }
    return "unknown";
}

char *value_to_display_string(Value v) {
    char buf[512];
    switch (v.type) {
        case VAL_NUMBER: {
            if (v.as.number == (long long)v.as.number) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v.as.number);
            } else {
                snprintf(buf, sizeof(buf), "%g", v.as.number);
            }
            return copy_cstr(buf);
        }
        case VAL_STRING:
            return copy_cstr(v.as.string ? v.as.string : "");
        case VAL_BOOL:
            return copy_cstr(v.as.boolean ? "true" : "false");
        case VAL_NONE:
            return copy_cstr("none");
        case VAL_ARRAY: {
            size_t cap = 256;
            char *out = malloc(cap);
            size_t len = 0;
            out[0] = '\0';

            #define APPEND(str) do { \
                size_t slen = strlen(str); \
                while (len + slen + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
                memcpy(out + len, str, slen + 1); \
                len += slen; \
            } while (0)

            APPEND("[");
            if (v.as.array) {
                for (int i = 0; i < v.as.array->count; i++) {
                    if (i > 0) APPEND(", ");
                    char *item_str = value_to_display_string(v.as.array->items[i]);
                    APPEND(item_str);
                    free(item_str);
                }
            }
            APPEND("]");

            #undef APPEND
            return out;
        }
        case VAL_MAP: {
            size_t cap = 256;
            char *out = malloc(cap);
            size_t len = 0;
            out[0] = '\0';

            #define APPEND_MAP(str) do { \
                size_t slen = strlen(str); \
                while (len + slen + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
                memcpy(out + len, str, slen + 1); \
                len += slen; \
            } while (0)

            APPEND_MAP("{");
            if (v.as.map) {
                for (int i = 0; i < v.as.map->count; i++) {
                    if (i > 0) APPEND_MAP(", ");
                    APPEND_MAP("\"");
                    APPEND_MAP(v.as.map->entries[i].key);
                    APPEND_MAP("\": ");
                    char *item_str = value_to_display_string(v.as.map->entries[i].value);
                    APPEND_MAP(item_str);
                    free(item_str);
                }
            }
            APPEND_MAP("}");

            #undef APPEND_MAP
            return out;
        }
        case VAL_STRUCT: {
            size_t cap = 256;
            char *out = malloc(cap);
            size_t len = 0;
            out[0] = '\0';

            #define APPEND_ST(str) do { \
                size_t slen = strlen(str); \
                while (len + slen + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
                memcpy(out + len, str, slen + 1); \
                len += slen; \
            } while (0)

            ObjInstance *inst = v.as.instance;
            if (inst && inst->struct_def) {
                APPEND_ST(inst->struct_def->name);
                APPEND_ST(" { ");
                for (int i = 0; i < inst->field_count; i++) {
                    if (i > 0) APPEND_ST(", ");
                    APPEND_ST(inst->struct_def->field_names[i]->chars);
                    APPEND_ST(": ");
                    char *fstr = value_to_display_string(inst->fields[i]);
                    APPEND_ST(fstr);
                    free(fstr);
                }
                APPEND_ST(" }");
            } else {
                APPEND_ST("<struct>");
            }

            #undef APPEND_ST
            return out;
        }
        case VAL_COROUTINE: {
            ObjCoroutine *co = v.as.coroutine;
            const char *state_str = "suspended";
            if (co) {
                if (co->state == CORO_RUNNING) state_str = "running";
                else if (co->state == CORO_DEAD) state_str = "dead";
            }
            snprintf(buf, sizeof(buf), "<coroutine %s (%s)>",
                     (co && co->func_name) ? co->func_name : "?", state_str);
            return copy_cstr(buf);
        }
    }
    return copy_cstr("");
}

void value_cleanup(void) {
    Obj *curr = gc_objects;
    while (curr != NULL) {
        Obj *next = curr->next;
        free_obj(curr);
        curr = next;
    }
    gc_objects = NULL;
    gc_object_count = 0;
    gc_live_bytes = 0;

    free(intern_entries);
    intern_entries = NULL;
    intern_count = 0;
    intern_tombstones = 0;
    intern_cap = 0;

    free(gc_temp_roots);
    gc_temp_roots = NULL;
    gc_temp_count = 0;
    gc_temp_cap = 0;

    free(registered_vms);
    registered_vms = NULL;
    registered_vm_count = 0;
    registered_vm_cap = 0;

    free(registered_envs);
    registered_envs = NULL;
    registered_env_count = 0;
    registered_env_cap = 0;

    free(registered_chunks);
    registered_chunks = NULL;
    registered_chunk_count = 0;
    registered_chunk_cap = 0;

    gc_clear_struct_defs();
}