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
#include "../include/disassemble.h"
#include "../include/repl.h"
#include "../include/module.h"

#define VESBO_VERSION "0.2.0"

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

static int write_bytecode(Chunk *chunk, const char *output_path) {
    FILE *f = fopen(output_path, "wb");
    if (!f) {
        fprintf(stderr, "Could not open output file: %s\n", output_path);
        return 0;
    }
    
    /* Write magic header */
    fprintf(f, "VSBX");
    
    /* Write code section */
    uint32_t code_len = (uint32_t)chunk->code_len;
    fwrite(&code_len, sizeof(uint32_t), 1, f);
    fwrite(chunk->code, 1, code_len, f);
    
    /* Write numbers section */
    uint32_t num_count = (uint32_t)chunk->numbers_len;
    fwrite(&num_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->numbers_len; i++) {
        fwrite(&chunk->numbers[i], sizeof(double), 1, f);
    }
    
    /* Write strings section */
    uint32_t str_count = (uint32_t)chunk->strings_len;
    fwrite(&str_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->strings_len; i++) {
        uint32_t str_len = (uint32_t)strlen(chunk->strings[i]);
        fwrite(&str_len, sizeof(uint32_t), 1, f);
        fwrite(chunk->strings[i], 1, str_len, f);
    }

    /* Write functions section */
    uint32_t func_count = (uint32_t)chunk->functions_len;
    fwrite(&func_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->functions_len; i++) {
        uint32_t name_len = (uint32_t)strlen(chunk->functions[i].name);
        fwrite(&name_len, sizeof(uint32_t), 1, f);
        fwrite(chunk->functions[i].name, 1, name_len, f);
        fwrite(&chunk->functions[i].code_offset, sizeof(uint32_t), 1, f);
        int32_t param_count = (int32_t)chunk->functions[i].param_count;
        fwrite(&param_count, sizeof(int32_t), 1, f);
    }

    fclose(f);
    printf("Compiled to %s\n", output_path);
    return 1;
}

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Vesbo %s — an English-readable programming language\n\n", VESBO_VERSION);
    fprintf(stderr, "Usage: %s [options] [file.vsb|file.vbo]\n\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -c, --compile            Compile .vsb source to .vbo bytecode\n");
    fprintf(stderr, "  -o, --output <file>      Output file (used with -c)\n");
    fprintf(stderr, "  -d, --disassemble        Disassemble .vsb source or .vbo bytecode\n");
    fprintf(stderr, "  -i, --repl               Start interactive REPL session\n");
    fprintf(stderr, "  -v, --version            Show version information\n");
    fprintf(stderr, "  -h, --help               Show this help message\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s                           Start interactive REPL\n", prog_name);
    fprintf(stderr, "  %s program.vsb               Run a Vesbo source file\n", prog_name);
    fprintf(stderr, "  %s -c program.vsb             Compile to program.vbo\n", prog_name);
    fprintf(stderr, "  %s -c program.vsb -o out.vbo  Compile with custom output\n", prog_name);
    fprintf(stderr, "  %s program.vbo                Run compiled bytecode\n", prog_name);
    fprintf(stderr, "  %s -d program.vsb             Disassemble Vesbo source\n", prog_name);
    fprintf(stderr, "  %s -d program.vbo             Disassemble compiled bytecode\n", prog_name);
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
    
    free(chunk->code);
    chunk->code = malloc(code_len > 0 ? code_len : 1);
    chunk->code_cap = code_len;
    chunk->code_len = code_len;
    if (code_len > 0 && fread(chunk->code, 1, code_len, f) != code_len) {
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
    
    free(chunk->numbers);
    chunk->numbers = malloc((num_count > 0 ? num_count : 1) * sizeof(double));
    chunk->numbers_cap = num_count;
    chunk->numbers_len = num_count;
    if (num_count > 0 && fread(chunk->numbers, sizeof(double), num_count, f) != num_count) {
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
    
    for (size_t i = 0; i < chunk->strings_len; i++) {
        free(chunk->strings[i]);
    }
    free(chunk->strings);
    chunk->strings = malloc((str_count > 0 ? str_count : 1) * sizeof(char *));
    chunk->strings_cap = str_count;
    chunk->strings_len = 0;
    
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
        chunk->strings[chunk->strings_len++] = str;
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
    atexit(module_system_cleanup);

    if (argc < 2) {
        repl_start();
        return 0;
    }

    int compile_mode = 0;
    int disassemble_mode = 0;
    int repl_mode = 0;
    const char *source_file = NULL;
    const char *output_file = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Vesbo %s\n", VESBO_VERSION);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--repl") == 0) {
            repl_mode = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--disassemble") == 0) {
            disassemble_mode = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) {
            compile_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: -o requires an output file argument\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            source_file = argv[i];
        }
    }

    if (repl_mode || (!source_file && !compile_mode && !disassemble_mode)) {
        repl_start();
        return 0;
    }

    if (!source_file) {
        fprintf(stderr, "Error: no input file provided\n\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Check if it's a bytecode file */
    size_t len = strlen(source_file);
    int is_bytecode = len > 4 && strcmp(source_file + len - 4, ".vbo") == 0;
    
    if (is_bytecode) {
        Chunk *chunk = load_bytecode(source_file);
        if (!chunk) {
            return 1;
        }
        
        if (disassemble_mode) {
            disassemble_chunk(chunk, source_file);
            chunk_free(chunk);
            return 0;
        }

        if (compile_mode) {
            fprintf(stderr, "Error: cannot compile an already compiled bytecode file\n");
            chunk_free(chunk);
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
    module_system_init();
    module_set_current_file(source_file);
    lexer_init(source);
    StmtList program = parse_program();

    if (compile_mode || disassemble_mode) {
        /* Compile to bytecode */
        CodeGen *gen = codegen_new();
        if (!gen) {
            fprintf(stderr, "Failed to create code generator\n");
            free_stmt_list(&program);
            free(source);
            return 1;
        }

        Chunk *chunk = codegen_compile(gen, program.items, program.count);
        if (!chunk) {
            fprintf(stderr, "Compilation failed\n");
            codegen_free(gen);
            free_stmt_list(&program);
            free(source);
            return 1;
        }

        if (disassemble_mode) {
            disassemble_chunk(chunk, source_file);
        }

        int ok = 1;
        if (compile_mode) {
            ok = write_bytecode(chunk, output_file);
        }

        codegen_free(gen);
        free_stmt_list(&program);
        free(source);
        return ok ? 0 : 1;
    } else {
        /* Interpret directly */
        interpret_program(program);
    }

    free(source);
    return 0;
}