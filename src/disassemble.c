#include "../include/disassemble.h"
#include <stdio.h>
#include <stdint.h>

static void print_function_header(Chunk *chunk, size_t offset) {
    for (size_t i = 0; i < chunk->functions_len; i++) {
        if (chunk->functions[i].code_offset == (uint32_t)offset) {
            printf("\n<%s> (params: %d):\n",
                   chunk->functions[i].name,
                   chunk->functions[i].param_count);
        }
    }
}

static uint32_t read_operand_at(Chunk *chunk, size_t offset) {
    size_t off = offset;
    return chunk_read_operand(chunk, &off);
}

size_t disassemble_instruction(Chunk *chunk, size_t offset) {
    if (offset >= chunk->code_len) return offset;

    print_function_header(chunk, offset);

    printf("%04zu  ", offset);
    uint8_t op = chunk->code[offset];

    switch (op) {
        case OP_PUSH_NUMBER: {
            uint32_t idx = read_operand_at(chunk, offset + 1);
            if (idx < chunk->numbers_len) {
                printf("%-20s %u (%.14g)\n", "OP_PUSH_NUMBER", idx, chunk->numbers[idx]);
            } else {
                printf("%-20s %u (invalid)\n", "OP_PUSH_NUMBER", idx);
            }
            return offset + 5;
        }

        case OP_PUSH_STRING: {
            uint32_t idx = read_operand_at(chunk, offset + 1);
            if (idx < chunk->strings_len) {
                printf("%-20s %u (\"%s\")\n", "OP_PUSH_STRING", idx, chunk->strings[idx]);
            } else {
                printf("%-20s %u (invalid)\n", "OP_PUSH_STRING", idx);
            }
            return offset + 5;
        }

        case OP_PUSH_TRUE:
            printf("%-20s\n", "OP_PUSH_TRUE");
            return offset + 1;

        case OP_PUSH_FALSE:
            printf("%-20s\n", "OP_PUSH_FALSE");
            return offset + 1;

        case OP_PUSH_NONE:
            printf("%-20s\n", "OP_PUSH_NONE");
            return offset + 1;

        case OP_PUSH_ARRAY: {
            uint32_t count = read_operand_at(chunk, offset + 1);
            printf("%-20s count: %u\n", "OP_PUSH_ARRAY", count);
            return offset + 5;
        }

        case OP_BUILD_MAP: {
            uint32_t count = read_operand_at(chunk, offset + 1);
            printf("%-20s count: %u\n", "OP_BUILD_MAP", count);
            return offset + 5;
        }

        case OP_DEFINE_LOCAL: {
            uint32_t slot = read_operand_at(chunk, offset + 1);
            printf("%-20s slot: %u\n", "OP_DEFINE_LOCAL", slot);
            return offset + 5;
        }

        case OP_GET_LOCAL: {
            uint32_t slot = read_operand_at(chunk, offset + 1);
            printf("%-20s slot: %u\n", "OP_GET_LOCAL", slot);
            return offset + 5;
        }

        case OP_SET_LOCAL: {
            uint32_t slot = read_operand_at(chunk, offset + 1);
            printf("%-20s slot: %u\n", "OP_SET_LOCAL", slot);
            return offset + 5;
        }

        case OP_GET_GLOBAL: {
            uint32_t idx = read_operand_at(chunk, offset + 1);
            if (idx < chunk->strings_len) {
                printf("%-20s %u (\"%s\")\n", "OP_GET_GLOBAL", idx, chunk->strings[idx]);
            } else {
                printf("%-20s %u\n", "OP_GET_GLOBAL", idx);
            }
            return offset + 5;
        }

        case OP_SET_GLOBAL: {
            uint32_t idx = read_operand_at(chunk, offset + 1);
            if (idx < chunk->strings_len) {
                printf("%-20s %u (\"%s\")\n", "OP_SET_GLOBAL", idx, chunk->strings[idx]);
            } else {
                printf("%-20s %u\n", "OP_SET_GLOBAL", idx);
            }
            return offset + 5;
        }

        case OP_ADD:
            printf("%-20s\n", "OP_ADD");
            return offset + 1;

        case OP_SUBTRACT:
            printf("%-20s\n", "OP_SUBTRACT");
            return offset + 1;

        case OP_MULTIPLY:
            printf("%-20s\n", "OP_MULTIPLY");
            return offset + 1;

        case OP_DIVIDE:
            printf("%-20s\n", "OP_DIVIDE");
            return offset + 1;

        case OP_MODULO:
            printf("%-20s\n", "OP_MODULO");
            return offset + 1;

        case OP_NEGATE:
            printf("%-20s\n", "OP_NEGATE");
            return offset + 1;

        case OP_EQUALS:
            printf("%-20s\n", "OP_EQUALS");
            return offset + 1;

        case OP_NOT_EQUALS:
            printf("%-20s\n", "OP_NOT_EQUALS");
            return offset + 1;

        case OP_LESS_THAN:
            printf("%-20s\n", "OP_LESS_THAN");
            return offset + 1;

        case OP_GREATER_THAN:
            printf("%-20s\n", "OP_GREATER_THAN");
            return offset + 1;

        case OP_LESS_THAN_EQUAL:
            printf("%-20s\n", "OP_LESS_THAN_EQUAL");
            return offset + 1;

        case OP_GREATER_THAN_EQUAL:
            printf("%-20s\n", "OP_GREATER_THAN_EQUAL");
            return offset + 1;

        case OP_AND:
            printf("%-20s\n", "OP_AND");
            return offset + 1;

        case OP_OR:
            printf("%-20s\n", "OP_OR");
            return offset + 1;

        case OP_NOT:
            printf("%-20s\n", "OP_NOT");
            return offset + 1;

        case OP_INDEX:
            printf("%-20s\n", "OP_INDEX");
            return offset + 1;

        case OP_INDEX_SET:
            printf("%-20s\n", "OP_INDEX_SET");
            return offset + 1;

        case OP_SLICE:
            printf("%-20s\n", "OP_SLICE");
            return offset + 1;

        case OP_JUMP: {
            uint32_t target = read_operand_at(chunk, offset + 1);
            printf("%-20s -> %04u\n", "OP_JUMP", target);
            return offset + 5;
        }

        case OP_JUMP_IF_FALSE: {
            uint32_t target = read_operand_at(chunk, offset + 1);
            printf("%-20s -> %04u\n", "OP_JUMP_IF_FALSE", target);
            return offset + 5;
        }

        case OP_LOOP:
            printf("%-20s\n", "OP_LOOP");
            return offset + 1;

        case OP_BREAK:
            printf("%-20s\n", "OP_BREAK");
            return offset + 1;

        case OP_CONTINUE:
            printf("%-20s\n", "OP_CONTINUE");
            return offset + 1;

        case OP_TRY_BEGIN: {
            uint32_t catch_target = read_operand_at(chunk, offset + 1);
            printf("%-20s catch -> %04u\n", "OP_TRY_BEGIN", catch_target);
            return offset + 5;
        }

        case OP_TRY_END:
            printf("%-20s\n", "OP_TRY_END");
            return offset + 1;

        case OP_CALL: {
            uint32_t func_idx = read_operand_at(chunk, offset + 1);
            if (func_idx < chunk->functions_len) {
                printf("%-20s [%u] <%s> (params: %d)\n",
                       "OP_CALL", func_idx,
                       chunk->functions[func_idx].name,
                       chunk->functions[func_idx].param_count);
            } else {
                printf("%-20s [%u] (invalid)\n", "OP_CALL", func_idx);
            }
            return offset + 5;
        }

        case OP_RETURN:
            printf("%-20s\n", "OP_RETURN");
            return offset + 1;

        case OP_DEFINE_FUNCTION: {
            uint32_t func_idx = read_operand_at(chunk, offset + 1);
            printf("%-20s [%u]\n", "OP_DEFINE_FUNCTION", func_idx);
            return offset + 5;
        }

        case OP_OUTPUT:
            printf("%-20s\n", "OP_OUTPUT");
            return offset + 1;

        case OP_INPUT:
            printf("%-20s\n", "OP_INPUT");
            return offset + 1;

        case OP_LENGTH:
            printf("%-20s\n", "OP_LENGTH");
            return offset + 1;

        case OP_LOWERCASE:
            printf("%-20s\n", "OP_LOWERCASE");
            return offset + 1;

        case OP_TRIM:
            printf("%-20s\n", "OP_TRIM");
            return offset + 1;

        case OP_NUMBER_CAST:
            printf("%-20s\n", "OP_NUMBER_CAST");
            return offset + 1;

        case OP_UPPERCASE:
            printf("%-20s\n", "OP_UPPERCASE");
            return offset + 1;

        case OP_STRING_CAST:
            printf("%-20s\n", "OP_STRING_CAST");
            return offset + 1;

        case OP_PUSH_BACK:
            printf("%-20s\n", "OP_PUSH_BACK");
            return offset + 1;

        case OP_TYPE_OF:
            printf("%-20s\n", "OP_TYPE_OF");
            return offset + 1;

        case OP_CALL_BUILTIN: {
            uint32_t str_idx = read_operand_at(chunk, offset + 1);
            uint32_t arg_count = read_operand_at(chunk, offset + 5);
            const char *bname = (str_idx < chunk->strings_len) ? chunk->strings[str_idx] : "unknown";
            printf("%-20s \"%s\" (args: %u)\n", "OP_CALL_BUILTIN", bname, arg_count);
            return offset + 9;
        }

        case OP_POP:
            printf("%-20s\n", "OP_POP");
            return offset + 1;

        case OP_HALT:
            printf("%-20s\n", "OP_HALT");
            return offset + 1;

        default:
            printf("UNKNOWN_OP (%02X)\n", op);
            return offset + 1;
    }
}

void disassemble_chunk(Chunk *chunk, const char *name) {
    printf("========================================\n");
    printf(" Bytecode Disassembly: %s\n", name ? name : "<unnamed>");
    printf("========================================\n");

    printf("\n-- Constant Numbers (%zu) --\n", chunk->numbers_len);
    for (size_t i = 0; i < chunk->numbers_len; i++) {
        printf("  [%zu] %g\n", i, chunk->numbers[i]);
    }

    printf("\n-- Constant Strings (%zu) --\n", chunk->strings_len);
    for (size_t i = 0; i < chunk->strings_len; i++) {
        printf("  [%zu] \"%s\"\n", i, chunk->strings[i]);
    }

    printf("\n-- Functions (%zu) --\n", chunk->functions_len);
    for (size_t i = 0; i < chunk->functions_len; i++) {
        printf("  [%zu] <%s> (offset: %04u, params: %d)\n",
               i, chunk->functions[i].name,
               chunk->functions[i].code_offset,
               chunk->functions[i].param_count);
    }

    printf("\n-- Instructions (%zu bytes) --\n", chunk->code_len);
    for (size_t offset = 0; offset < chunk->code_len;) {
        offset = disassemble_instruction(chunk, offset);
    }
    printf("========================================\n\n");
}
