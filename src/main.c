#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/interpreter.h"
#include "../include/value.h"
#include "../include/codegen.h"
#include "../include/vm.h"
#include "../include/bytecode.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {
        fprintf(stderr, "Could not determine file size: %s\n", path);
        fclose(f);
        exit(1);
    }
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Could not allocate memory for: %s\n", path);
        fclose(f);
        exit(1);
    }
    size_t bytes_read = fread(buffer, 1, (size_t)size, f);
    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Could not read file: %s\n", path);
        free(buffer);
        fclose(f);
        exit(1);
    }
    buffer[size] = '\0';
    fclose(f);
    return buffer;
}

static void write_bytecode(Chunk *chunk, const char *output_path) {
    FILE *f = fopen(output_path, "wb");
    if (!f) {
        fprintf(stderr, "Could not open output file: %s\n", output_path);
        return;
    }
    
    /* Write magic header */
    fprintf(f, "VSBX");
    
    /* Write code section */
    uint32_t code_len = chunk->code_len;
    fwrite(&code_len, sizeof(uint32_t), 1, f);
    fwrite(chunk->code, 1, code_len, f);
    
    /* Write numbers section */
    uint32_t num_count = chunk->numbers_len;
    fwrite(&num_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->numbers_len; i++) {
        fwrite(&chunk->numbers[i], sizeof(double), 1, f);
    }
    
    /* Write strings section */
    uint32_t str_count = chunk->strings_len;
    fwrite(&str_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->strings_len; i++) {
        uint32_t str_len = strlen(chunk->strings[i]);
        fwrite(&str_len, sizeof(uint32_t), 1, f);
        fwrite(chunk->strings[i], 1, str_len, f);
    }

    /* Write functions section (previously omitted entirely, which meant
     * every loaded .vbo file had no functions and could never call main) */
    uint32_t func_count = chunk->functions_len;
    fwrite(&func_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->functions_len; i++) {
        uint32_t name_len = strlen(chunk->functions[i].name);
        fwrite(&name_len, sizeof(uint32_t), 1, f);
        fwrite(chunk->functions[i].name, 1, name_len, f);
        fwrite(&chunk->functions[i].code_offset, sizeof(uint32_t), 1, f);
        int32_t param_count = chunk->functions[i].param_count;
        fwrite(&param_count, sizeof(int32_t), 1, f);
    }

    fclose(f);
    printf("Compiled to %s\n", output_path);
}

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [options] <file.vsb|file.vbo>\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -c, --compile <output>    Compile to bytecode file\n");
    fprintf(stderr, "  -o <output>               Output file (with -c)\n");
}

static Chunk *load_bytecode(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open bytecode file: %s\n", path);
        return NULL;
    }
    
    Chunk *chunk = chunk_new();
    if (!chunk) {
        fclose(f);
        return NULL;
    }
    
    /* Check magic header */
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || strncmp(magic, "VSBX", 4) != 0) {
        fprintf(stderr, "Invalid bytecode file: %s\n", path);
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    
    /* Read code section */
    uint32_t code_len;
    if (fread(&code_len, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "Invalid bytecode file: cannot read code length\n");
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    
    chunk->code = malloc(code_len);
    chunk->code_cap = code_len;
    chunk->code_len = code_len;
    if (fread(chunk->code, 1, code_len, f) != code_len) {
        fprintf(stderr, "Invalid bytecode file: cannot read code\n");
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    
    /* Read numbers section */
    uint32_t num_count;
    if (fread(&num_count, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "Invalid bytecode file: cannot read number count\n");
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    
    chunk->numbers = malloc(num_count * sizeof(double));
    chunk->numbers_cap = num_count;
    chunk->numbers_len = num_count;
    if (fread(chunk->numbers, sizeof(double), num_count, f) != num_count) {
        fprintf(stderr, "Invalid bytecode file: cannot read numbers\n");
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    
    /* Read strings section */
    uint32_t str_count;
    if (fread(&str_count, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "Invalid bytecode file: cannot read string count\n");
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    
    chunk->strings = malloc(str_count * sizeof(char *));
    chunk->strings_cap = str_count;
    chunk->strings_len = str_count;
    
    for (uint32_t i = 0; i < str_count; i++) {
        uint32_t str_len;
        if (fread(&str_len, sizeof(uint32_t), 1, f) != 1) {
            fprintf(stderr, "Invalid bytecode file: cannot read string length\n");
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }
        
        char *str = malloc(str_len + 1);
        if (fread(str, 1, str_len, f) != str_len) {
            fprintf(stderr, "Invalid bytecode file: cannot read string\n");
            free(str);
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }
        str[str_len] = '\0';
        chunk->strings[i] = str;
    }

    /* Read functions section */
    uint32_t func_count;
    if (fread(&func_count, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "Invalid bytecode file: cannot read function count\n");
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    for (uint32_t i = 0; i < func_count; i++) {
        uint32_t name_len;
        if (fread(&name_len, sizeof(uint32_t), 1, f) != 1) {
            fprintf(stderr, "Invalid bytecode file: cannot read function name length\n");
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }

        char *name = malloc(name_len + 1);
        if (fread(name, 1, name_len, f) != name_len) {
            fprintf(stderr, "Invalid bytecode file: cannot read function name\n");
            free(name);
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }
        name[name_len] = '\0';

        uint32_t code_offset;
        int32_t param_count;
        if (fread(&code_offset, sizeof(uint32_t), 1, f) != 1 ||
            fread(&param_count, sizeof(int32_t), 1, f) != 1) {
            fprintf(stderr, "Invalid bytecode file: cannot read function metadata\n");
            free(name);
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }

        chunk_add_function(chunk, name, code_offset, param_count);
        free(name);
    }

    fclose(f);
    return chunk;
}

int main(int argc, char **argv) {
    atexit(value_cleanup);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    int compile_mode = 0;
    const char *source_file = NULL;
    const char *output_file = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) {
            compile_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            }
        } else if (argv[i][0] != '-') {
            source_file = argv[i];
        }
    }

    if (!source_file) {
        print_usage(argv[0]);
        return 1;
    }

    /* Check if it's a bytecode file to run */
    size_t len = strlen(source_file);
    int is_bytecode = len > 4 && strcmp(source_file + len - 4, ".vbo") == 0;
    
    if (is_bytecode && !compile_mode) {
        /* Load and run bytecode directly */
        Chunk *chunk = load_bytecode(source_file);
        if (!chunk) {
            return 1;
        }
        
        VM *vm = vm_new(chunk);
        if (!vm) {
            fprintf(stderr, "Failed to create virtual machine\n");
            chunk_free(chunk);
            return 1;
        }
        
        int result = vm_execute(vm);
        vm_free(vm);
        chunk_free(chunk);
        return result;
    }

    /* Generate output file name if not provided */
    char default_output[256];
    if (compile_mode && !output_file) {
        strcpy(default_output, source_file);
        char *dot = strrchr(default_output, '.');
        if (dot) *dot = '\0';
        strcat(default_output, ".vbo");
        output_file = default_output;
    }

    char *source = read_file(source_file);
    lexer_init(source);
    StmtList program = parse_program();

    if (compile_mode) {
        /* Compile to bytecode */
        CodeGen *gen = codegen_new();
        if (!gen) {
            fprintf(stderr, "Failed to create code generator\n");
            free(source);
            return 1;
        }

        Chunk *chunk = codegen_compile(gen, program.items, program.count);
        if (!chunk) {
            fprintf(stderr, "Compilation failed\n");
            codegen_free(gen);
            free(source);
            return 1;
        }

        write_bytecode(chunk, output_file);
        codegen_free(gen);
    } else {
        /* Interpret directly */
        interpret_program(program);
    }

    free(source);
    return 0;
}