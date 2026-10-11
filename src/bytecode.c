#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/bytecode.h"

#define INITIAL_CAPACITY 256

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

Chunk *chunk_new(void) {
    Chunk *chunk = malloc(sizeof(Chunk));
    if (!chunk) return NULL;
    memset(chunk, 0, sizeof(Chunk));

    chunk->code = malloc(INITIAL_CAPACITY);
    chunk->code_cap = INITIAL_CAPACITY;
    chunk->code_len = 0;

    chunk->numbers = malloc(INITIAL_CAPACITY * sizeof(double));
    chunk->numbers_cap = INITIAL_CAPACITY;
    chunk->numbers_len = 0;

    chunk->strings = malloc(INITIAL_CAPACITY * sizeof(char *));
    chunk->string_objs = calloc(INITIAL_CAPACITY, sizeof(ObjString *));
    chunk->strings_cap = INITIAL_CAPACITY;
    chunk->strings_len = 0;

    chunk->functions = malloc(INITIAL_CAPACITY * sizeof(FunctionDef));
    chunk->functions_cap = INITIAL_CAPACITY;
    chunk->functions_len = 0;

    chunk->global_names = malloc(64 * sizeof(char *));
    chunk->globals_cap = 64;
    chunk->globals_len = 0;

    chunk->structs = malloc(16 * sizeof(ChunkStructDef));
    chunk->structs_cap = 16;
    chunk->structs_len = 0;

    chunk->locs = malloc(INITIAL_CAPACITY * sizeof(BytecodeLoc));
    chunk->locs_cap = INITIAL_CAPACITY;
    chunk->locs_len = 0;

    if (!chunk->code || !chunk->numbers || !chunk->strings || !chunk->string_objs ||
        !chunk->functions || !chunk->global_names || !chunk->structs || !chunk->locs) {
        chunk_free(chunk);
        return NULL;
    }

    gc_register_chunk(chunk);
    return chunk;
}

void chunk_free(Chunk *chunk) {
    if (!chunk) return;
    gc_unregister_chunk(chunk);

    free(chunk->code);

    for (size_t i = 0; i < chunk->strings_len; i++) {
        free(chunk->strings[i]);
    }
    free(chunk->strings);
    free(chunk->string_objs);

    for (size_t i = 0; i < chunk->functions_len; i++) {
        free(chunk->functions[i].name);
        free(chunk->functions[i].file);
    }
    free(chunk->functions);

    for (size_t i = 0; i < chunk->globals_len; i++) {
        free(chunk->global_names[i]);
    }
    free(chunk->global_names);

    for (size_t i = 0; i < chunk->structs_len; i++) {
        free(chunk->structs[i].name);
        for (int j = 0; j < chunk->structs[i].field_count; j++) {
            free(chunk->structs[i].field_names[j]);
        }
        free(chunk->structs[i].field_names);
        free(chunk->structs[i].field_defaults);
        /* Note: runtime_def is registered with gc_register_struct_def and freed on value_cleanup */
    }
    free(chunk->structs);

    free(chunk->locs);
    free(chunk->numbers);
    free(chunk);
}

void chunk_set_location(Chunk *chunk, const char *file, int line, int column) {
    if (!chunk) return;
    if (file) {
        chunk->cur_file_idx = (uint32_t)chunk_add_string(chunk, file);
    } else if (!chunk->has_cur_loc) {
        chunk->cur_file_idx = (uint32_t)chunk_add_string(chunk, "<script>");
    }
    if (line > 0) chunk->cur_line = line;
    if (column > 0) chunk->cur_column = column;
    chunk->has_cur_loc = 1;
}

int chunk_get_location(Chunk *chunk, uint32_t offset, const char **out_file, int *out_line, int *out_col) {
    if (!chunk || chunk->locs_len == 0) {
        if (out_file) *out_file = "<script>";
        if (out_line) *out_line = 0;
        if (out_col) *out_col = 0;
        return 0;
    }
    /* Binary search for last loc with code_offset <= offset */
    size_t lo = 0;
    size_t hi = chunk->locs_len - 1;
    size_t best = 0;
    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (chunk->locs[mid].code_offset <= offset) {
            best = mid;
            lo = mid + 1;
        } else {
            if (mid == 0) break;
            hi = mid - 1;
        }
    }
    BytecodeLoc *loc = &chunk->locs[best];
    if (out_file) {
        *out_file = (loc->file_str_idx < chunk->strings_len) ? chunk->strings[loc->file_str_idx] : "<script>";
    }
    if (out_line) *out_line = loc->line;
    if (out_col) *out_col = loc->column;
    return 1;
}

int chunk_find_enclosing_function(Chunk *chunk, uint32_t offset) {
    if (!chunk) return -1;
    int best_idx = -1;
    uint32_t best_offset = 0;
    for (size_t i = 0; i < chunk->functions_len; i++) {
        uint32_t foff = chunk->functions[i].code_offset;
        if (foff <= offset && (best_idx < 0 || foff >= best_offset)) {
            best_idx = (int)i;
            best_offset = foff;
        }
    }
    return best_idx;
}

static void ensure_code_capacity(Chunk *chunk, size_t needed) {
    while (chunk->code_len + needed > chunk->code_cap) {
        chunk->code_cap *= 2;
        uint8_t *new_code = realloc(chunk->code, chunk->code_cap);
        if (!new_code) {
            free(chunk->code);
            chunk->code = NULL;
            return;
        }
        chunk->code = new_code;
    }
}

void chunk_write_op(Chunk *chunk, uint8_t op) {
    if (chunk->has_cur_loc && chunk->cur_line > 0) {
        int need_loc = 1;
        if (chunk->locs_len > 0) {
            BytecodeLoc *last = &chunk->locs[chunk->locs_len - 1];
            if (last->file_str_idx == chunk->cur_file_idx &&
                last->line == chunk->cur_line &&
                last->column == chunk->cur_column) {
                need_loc = 0;
            }
        }
        if (need_loc) {
            if (chunk->locs_len >= chunk->locs_cap) {
                chunk->locs_cap *= 2;
                chunk->locs = realloc(chunk->locs, chunk->locs_cap * sizeof(BytecodeLoc));
            }
            chunk->locs[chunk->locs_len].code_offset = (uint32_t)chunk->code_len;
            chunk->locs[chunk->locs_len].file_str_idx = chunk->cur_file_idx;
            chunk->locs[chunk->locs_len].line = chunk->cur_line;
            chunk->locs[chunk->locs_len].column = chunk->cur_column;
            chunk->locs_len++;
        }
    }
    ensure_code_capacity(chunk, 1);
    chunk->code[chunk->code_len++] = op;
}

void chunk_write_operand(Chunk *chunk, uint32_t operand) {
    ensure_code_capacity(chunk, 4);
    chunk->code[chunk->code_len++] = (operand >> 24) & 0xFF;
    chunk->code[chunk->code_len++] = (operand >> 16) & 0xFF;
    chunk->code[chunk->code_len++] = (operand >> 8) & 0xFF;
    chunk->code[chunk->code_len++] = operand & 0xFF;
}

size_t chunk_add_number(Chunk *chunk, double value) {
    for (size_t i = 0; i < chunk->numbers_len; i++) {
        if (chunk->numbers[i] == value) {
            return i;
        }
    }
    if (chunk->numbers_len >= chunk->numbers_cap) {
        chunk->numbers_cap *= 2;
        double *new_nums = realloc(chunk->numbers, chunk->numbers_cap * sizeof(double));
        if (!new_nums) return (size_t)-1;
        chunk->numbers = new_nums;
    }
    chunk->numbers[chunk->numbers_len] = value;
    return chunk->numbers_len++;
}

size_t chunk_add_string(Chunk *chunk, const char *str) {
    if (!str) str = "";
    for (size_t i = 0; i < chunk->strings_len; i++) {
        if (strcmp(chunk->strings[i], str) == 0) {
            return i;
        }
    }
    if (chunk->strings_len >= chunk->strings_cap) {
        size_t old_cap = chunk->strings_cap;
        chunk->strings_cap *= 2;
        char **new_strs = realloc(chunk->strings, chunk->strings_cap * sizeof(char *));
        ObjString **new_objs = realloc(chunk->string_objs, chunk->strings_cap * sizeof(ObjString *));
        if (!new_strs || !new_objs) return (size_t)-1;
        memset(new_objs + old_cap, 0, (chunk->strings_cap - old_cap) * sizeof(ObjString *));
        chunk->strings = new_strs;
        chunk->string_objs = new_objs;
    }

    char *copy = dup_str(str);
    if (!copy) return (size_t)-1;

    size_t idx = chunk->strings_len++;
    chunk->strings[idx] = copy;
    chunk->string_objs[idx] = NULL;
    chunk->string_objs[idx] = intern_string(copy, strlen(copy));
    return idx;
}

int chunk_find_global(Chunk *chunk, const char *name) {
    if (!chunk || !name) return -1;
    for (size_t i = 0; i < chunk->globals_len; i++) {
        if (strcmp(chunk->global_names[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

uint32_t chunk_resolve_global(Chunk *chunk, const char *name) {
    int existing = chunk_find_global(chunk, name);
    if (existing >= 0) return (uint32_t)existing;

    if (chunk->globals_len >= chunk->globals_cap) {
        chunk->globals_cap *= 2;
        chunk->global_names = realloc(chunk->global_names, chunk->globals_cap * sizeof(char *));
    }
    chunk->global_names[chunk->globals_len] = dup_str(name);
    return (uint32_t)chunk->globals_len++;
}

int chunk_find_struct(Chunk *chunk, const char *name) {
    if (!chunk || !name) return -1;
    for (size_t i = 0; i < chunk->structs_len; i++) {
        if (strcmp(chunk->structs[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int chunk_add_struct(Chunk *chunk, const char *name, int field_count, char **field_names, Value *field_defaults) {
    int existing = chunk_find_struct(chunk, name);
    if (existing >= 0) {
        ChunkStructDef *s = &chunk->structs[existing];
        if (s->field_count == 0 && field_count > 0) {
            s->field_count = field_count;
            s->field_names = malloc((size_t)field_count * sizeof(char *));
            s->field_defaults = malloc((size_t)field_count * sizeof(Value));
            for (int i = 0; i < field_count; i++) {
                s->field_names[i] = dup_str(field_names[i]);
                s->field_defaults[i] = field_defaults ? field_defaults[i] : make_none_value();
            }
        }
        return existing;
    }

    if (chunk->structs_len >= chunk->structs_cap) {
        chunk->structs_cap *= 2;
        chunk->structs = realloc(chunk->structs, chunk->structs_cap * sizeof(ChunkStructDef));
    }

    size_t idx = chunk->structs_len++;
    ChunkStructDef *s = &chunk->structs[idx];
    s->name = dup_str(name);
    s->field_count = field_count;
    s->field_names = field_count > 0 ? malloc((size_t)field_count * sizeof(char *)) : NULL;
    s->field_defaults = field_count > 0 ? malloc((size_t)field_count * sizeof(Value)) : NULL;
    for (int i = 0; i < field_count; i++) {
        s->field_names[i] = dup_str(field_names[i]);
        s->field_defaults[i] = field_defaults ? field_defaults[i] : make_none_value();
    }
    s->runtime_def = NULL;
    return (int)idx;
}

StructDef *chunk_get_runtime_struct(Chunk *chunk, int struct_idx) {
    if (!chunk || struct_idx < 0 || (size_t)struct_idx >= chunk->structs_len) return NULL;
    ChunkStructDef *cs = &chunk->structs[struct_idx];
    if (cs->runtime_def) return cs->runtime_def;

    StructDef *def = struct_def_new(cs->name, cs->field_count);
    for (int i = 0; i < cs->field_count; i++) {
        def->field_names[i] = intern_string(cs->field_names[i], strlen(cs->field_names[i]));
        def->field_defaults[i] = cs->field_defaults[i];
    }

    /* Bind any methods compiled as "<StructName>.<method>" in chunk->functions */
    size_t prefix_len = strlen(cs->name);
    for (size_t i = 0; i < chunk->functions_len; i++) {
        const char *fname = chunk->functions[i].name;
        if (strncmp(fname, cs->name, prefix_len) == 0 && fname[prefix_len] == '.') {
            const char *mname = fname + prefix_len + 1;
            struct_def_add_method(def, mname, (int)i, NULL);
        }
    }

    gc_register_struct_def(def);
    cs->runtime_def = def;
    return def;
}

void chunk_patch_operand(Chunk *chunk, size_t operand_offset, uint32_t value) {
    chunk->code[operand_offset] = (value >> 24) & 0xFF;
    chunk->code[operand_offset + 1] = (value >> 16) & 0xFF;
    chunk->code[operand_offset + 2] = (value >> 8) & 0xFF;
    chunk->code[operand_offset + 3] = value & 0xFF;
}

uint32_t chunk_read_operand(Chunk *chunk, size_t *offset) {
    if (*offset + 4 > chunk->code_len) return 0;

    uint32_t value = 0;
    value |= ((uint32_t)chunk->code[*offset]) << 24;
    value |= ((uint32_t)chunk->code[*offset + 1]) << 16;
    value |= ((uint32_t)chunk->code[*offset + 2]) << 8;
    value |= (uint32_t)chunk->code[*offset + 3];

    *offset += 4;
    return value;
}

int chunk_add_function_loc(Chunk *chunk, const char *name, uint32_t code_offset, int param_count,
                           const char *file, int line, int column) {
    if (chunk->functions_len >= chunk->functions_cap) {
        chunk->functions_cap *= 2;
        FunctionDef *new_funcs = realloc(chunk->functions, chunk->functions_cap * sizeof(FunctionDef));
        if (!new_funcs) return -1;
        chunk->functions = new_funcs;
    }

    char *name_copy = dup_str(name);
    if (!name_copy) return -1;

    size_t idx = chunk->functions_len++;
    chunk->functions[idx].name = name_copy;
    chunk->functions[idx].code_offset = code_offset;
    chunk->functions[idx].param_count = param_count;
    chunk->functions[idx].file = dup_str(file ? file : "<script>");
    chunk->functions[idx].line = line;
    chunk->functions[idx].column = column;

    return (int)idx;
}

int chunk_add_function(Chunk *chunk, const char *name, uint32_t code_offset, int param_count) {
    const char *file = "<script>";
    if (chunk->has_cur_loc && chunk->cur_file_idx < chunk->strings_len) {
        file = chunk->strings[chunk->cur_file_idx];
    }
    return chunk_add_function_loc(chunk, name, code_offset, param_count,
                                  file, chunk->cur_line, chunk->cur_column);
}

int chunk_find_function(Chunk *chunk, const char *name) {
    for (size_t i = 0; i < chunk->functions_len; i++) {
        if (strcmp(chunk->functions[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}