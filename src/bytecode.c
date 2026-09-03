#include <stdlib.h>
#include <string.h>
#include "../include/bytecode.h"

#define INITIAL_CAPACITY 256

Chunk *chunk_new(void) {
    Chunk *chunk = malloc(sizeof(Chunk));
    if (!chunk) return NULL;
    
    chunk->code = malloc(INITIAL_CAPACITY);
    chunk->code_cap = INITIAL_CAPACITY;
    chunk->code_len = 0;
    
    chunk->numbers = malloc(INITIAL_CAPACITY * sizeof(double));
    chunk->numbers_cap = INITIAL_CAPACITY;
    chunk->numbers_len = 0;
    
    chunk->strings = malloc(INITIAL_CAPACITY * sizeof(char *));
    chunk->strings_cap = INITIAL_CAPACITY;
    chunk->strings_len = 0;
    
    chunk->functions = malloc(INITIAL_CAPACITY * sizeof(FunctionDef));
    chunk->functions_cap = INITIAL_CAPACITY;
    chunk->functions_len = 0;
    
    if (!chunk->code || !chunk->numbers || !chunk->strings || !chunk->functions) {
        free(chunk->code);
        free(chunk->numbers);
        free(chunk->strings);
        free(chunk->functions);
        free(chunk);
        return NULL;
    }
    
    return chunk;
}

void chunk_free(Chunk *chunk) {
    if (!chunk) return;
    
    free(chunk->code);
    
    for (size_t i = 0; i < chunk->strings_len; i++) {
        free(chunk->strings[i]);
    }
    free(chunk->strings);
    
    for (size_t i = 0; i < chunk->functions_len; i++) {
        free(chunk->functions[i].name);
    }
    free(chunk->functions);
    
    free(chunk->numbers);
    free(chunk);
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
    if (chunk->numbers_len >= chunk->numbers_cap) {
        chunk->numbers_cap *= 2;
        double *new_nums = realloc(chunk->numbers, chunk->numbers_cap * sizeof(double));
        if (!new_nums) return -1;
        chunk->numbers = new_nums;
    }
    chunk->numbers[chunk->numbers_len] = value;
    return chunk->numbers_len++;
}

size_t chunk_add_string(Chunk *chunk, const char *str) {
    if (chunk->strings_len >= chunk->strings_cap) {
        chunk->strings_cap *= 2;
        char **new_strs = realloc(chunk->strings, chunk->strings_cap * sizeof(char *));
        if (!new_strs) return -1;
        chunk->strings = new_strs;
    }
    
    char *copy = malloc(strlen(str) + 1);
    if (!copy) return -1;
    strcpy(copy, str);
    
    chunk->strings[chunk->strings_len] = copy;
    return chunk->strings_len++;
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

int chunk_add_function(Chunk *chunk, const char *name, uint32_t code_offset, int param_count) {
    if (chunk->functions_len >= chunk->functions_cap) {
        chunk->functions_cap *= 2;
        FunctionDef *new_funcs = realloc(chunk->functions, chunk->functions_cap * sizeof(FunctionDef));
        if (!new_funcs) return -1;
        chunk->functions = new_funcs;
    }
    
    char *name_copy = malloc(strlen(name) + 1);
    if (!name_copy) return -1;
    strcpy(name_copy, name);
    
    chunk->functions[chunk->functions_len].name = name_copy;
    chunk->functions[chunk->functions_len].code_offset = code_offset;
    chunk->functions[chunk->functions_len].param_count = param_count;
    
    return chunk->functions_len++;
}

int chunk_find_function(Chunk *chunk, const char *name) {
    for (size_t i = 0; i < chunk->functions_len; i++) {
        if (strcmp(chunk->functions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}