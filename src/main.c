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
#include "../include/vesbo.h"

#define VESBO_VERSION "0.2.0"

static void maybe_print_profile_at_exit(void) {
    if (gc_get_profile()) {
        gc_print_profile_report();
    }
}

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

    char *buffer = malloc((size_t)size + 1);
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

static void write_str(FILE *f, const char *s) {
    if (!s) s = "";
    uint32_t len = (uint32_t)strlen(s);
    fwrite(&len, sizeof(uint32_t), 1, f);
    if (len > 0) fwrite(s, 1, len, f);
}

static char *read_str(FILE *f) {
    uint32_t len = 0;
    if (fread(&len, sizeof(uint32_t), 1, f) != 1) return NULL;
    char *s = malloc((size_t)len + 1);
    if (!s) return NULL;
    if (len > 0 && fread(s, 1, len, f) != len) {
        free(s);
        return NULL;
    }
    s[len] = '\0';
    return s;
}

static int write_bytecode(Chunk *chunk, const char *output_path) {
    FILE *f = fopen(output_path, "wb");
    if (!f) {
        fprintf(stderr, "Could not open output file: %s\n", output_path);
        return 0;
    }

    /* Write magic header */
    fprintf(f, "VSB2");

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
        write_str(f, chunk->strings[i]);
    }

    /* Write functions section */
    uint32_t func_count = (uint32_t)chunk->functions_len;
    fwrite(&func_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->functions_len; i++) {
        write_str(f, chunk->functions[i].name);
        fwrite(&chunk->functions[i].code_offset, sizeof(uint32_t), 1, f);
        int32_t param_count = (int32_t)chunk->functions[i].param_count;
        fwrite(&param_count, sizeof(int32_t), 1, f);
        write_str(f, chunk->functions[i].file);
        int32_t line = (int32_t)chunk->functions[i].line;
        int32_t col = (int32_t)chunk->functions[i].column;
        fwrite(&line, sizeof(int32_t), 1, f);
        fwrite(&col, sizeof(int32_t), 1, f);
    }

    /* Write globals section */
    uint32_t glob_count = (uint32_t)chunk->globals_len;
    fwrite(&glob_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->globals_len; i++) {
        write_str(f, chunk->global_names[i]);
    }

    /* Write structs section */
    uint32_t struct_count = (uint32_t)chunk->structs_len;
    fwrite(&struct_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->structs_len; i++) {
        ChunkStructDef *s = &chunk->structs[i];
        write_str(f, s->name);
        int32_t fcount = (int32_t)s->field_count;
        fwrite(&fcount, sizeof(int32_t), 1, f);
        for (int j = 0; j < s->field_count; j++) {
            write_str(f, s->field_names[j]);
            Value dv = s->field_defaults[j];
            uint8_t tag = (uint8_t)dv.type;
            fwrite(&tag, sizeof(uint8_t), 1, f);
            if (dv.type == VAL_NUMBER) {
                fwrite(&dv.as.number, sizeof(double), 1, f);
            } else if (dv.type == VAL_BOOL) {
                uint8_t b = (uint8_t)(dv.as.boolean ? 1 : 0);
                fwrite(&b, sizeof(uint8_t), 1, f);
            } else if (dv.type == VAL_STRING) {
                write_str(f, dv.as.string);
            }
        }
    }

    /* Write source locations section */
    uint32_t loc_count = (uint32_t)chunk->locs_len;
    fwrite(&loc_count, sizeof(uint32_t), 1, f);
    for (size_t i = 0; i < chunk->locs_len; i++) {
        fwrite(&chunk->locs[i].code_offset, sizeof(uint32_t), 1, f);
        fwrite(&chunk->locs[i].file_str_idx, sizeof(uint32_t), 1, f);
        int32_t line = (int32_t)chunk->locs[i].line;
        int32_t col = (int32_t)chunk->locs[i].column;
        fwrite(&line, sizeof(int32_t), 1, f);
        fwrite(&col, sizeof(int32_t), 1, f);
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
    fprintf(stderr, "  --vm                     Run .vsb source directly in the bytecode VM\n");
    fprintf(stderr, "  --gc-stress              Trigger garbage collection on every allocation\n");
    fprintf(stderr, "  --profile                Print instruction and allocation profile at exit\n");
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

    char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        fprintf(stderr, "Invalid bytecode file: %s\n", path);
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    if (strncmp(magic, "VSBX", 4) == 0) {
        fprintf(stderr, "Outdated bytecode format (VSBX) in '%s'; please recompile with 'vesbo -c'\n", path);
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    if (strncmp(magic, "VSB2", 4) != 0) {
        fprintf(stderr, "Invalid bytecode file: %s\n", path);
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }
    int is_v2 = 1;

    uint32_t code_len;
    if (fread(&code_len, sizeof(uint32_t), 1, f) != 1) {
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    free(chunk->code);
    chunk->code = malloc(code_len > 0 ? code_len : 1);
    chunk->code_cap = code_len > 0 ? code_len : 1;
    chunk->code_len = code_len;
    if (code_len > 0 && fread(chunk->code, 1, code_len, f) != code_len) {
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    uint32_t num_count;
    if (fread(&num_count, sizeof(uint32_t), 1, f) != 1) {
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    free(chunk->numbers);
    chunk->numbers = malloc((num_count > 0 ? num_count : 1) * sizeof(double));
    chunk->numbers_cap = num_count > 0 ? num_count : 1;
    chunk->numbers_len = num_count;
    if (num_count > 0 && fread(chunk->numbers, sizeof(double), num_count, f) != num_count) {
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    uint32_t str_count;
    if (fread(&str_count, sizeof(uint32_t), 1, f) != 1) {
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    for (size_t i = 0; i < chunk->strings_len; i++) free(chunk->strings[i]);
    free(chunk->strings);
    free(chunk->string_objs);
    size_t scap = str_count > 0 ? str_count : 1;
    chunk->strings = malloc(scap * sizeof(char *));
    chunk->string_objs = calloc(scap, sizeof(ObjString *));
    chunk->strings_cap = scap;
    chunk->strings_len = 0;

    for (uint32_t i = 0; i < str_count; i++) {
        char *str = read_str(f);
        if (!str) {
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }
        size_t idx = chunk->strings_len++;
        chunk->strings[idx] = str;
        chunk->string_objs[idx] = intern_string(str, strlen(str));
    }

    uint32_t func_count;
    if (fread(&func_count, sizeof(uint32_t), 1, f) != 1) {
        chunk_free(chunk);
        fclose(f);
        return NULL;
    }

    for (uint32_t i = 0; i < func_count; i++) {
        char *name = read_str(f);
        if (!name) {
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }
        uint32_t code_offset = 0;
        int32_t param_count = 0;
        if (fread(&code_offset, sizeof(uint32_t), 1, f) != 1 ||
            fread(&param_count, sizeof(int32_t), 1, f) != 1) {
            free(name);
            chunk_free(chunk);
            fclose(f);
            return NULL;
        }
        char *file = NULL;
        int32_t line = 0, col = 0;
        if (is_v2) {
            file = read_str(f);
            if (!file ||
                fread(&line, sizeof(int32_t), 1, f) != 1 ||
                fread(&col, sizeof(int32_t), 1, f) != 1) {
                free(name);
                free(file);
                chunk_free(chunk);
                fclose(f);
                return NULL;
            }
        }
        chunk_add_function_loc(chunk, name, code_offset, param_count, file, line, col);
        free(name);
        free(file);
    }

    if (is_v2) {
        /* Read globals */
        uint32_t glob_count = 0;
        if (fread(&glob_count, sizeof(uint32_t), 1, f) == 1) {
            for (uint32_t i = 0; i < glob_count; i++) {
                char *gname = read_str(f);
                if (gname) {
                    chunk_resolve_global(chunk, gname);
                    free(gname);
                }
            }
        }

        /* Read structs */
        uint32_t struct_count = 0;
        if (fread(&struct_count, sizeof(uint32_t), 1, f) == 1) {
            for (uint32_t i = 0; i < struct_count; i++) {
                char *sname = read_str(f);
                int32_t fcount = 0;
                if (!sname || fread(&fcount, sizeof(int32_t), 1, f) != 1) {
                    free(sname);
                    break;
                }
                char **fnames = fcount > 0 ? malloc((size_t)fcount * sizeof(char *)) : NULL;
                Value *fdefaults = fcount > 0 ? malloc((size_t)fcount * sizeof(Value)) : NULL;
                for (int32_t j = 0; j < fcount; j++) {
                    fnames[j] = read_str(f);
                    uint8_t tag = (uint8_t)VAL_NONE;
                    fread(&tag, sizeof(uint8_t), 1, f);
                    if (tag == (uint8_t)VAL_NUMBER) {
                        double num = 0.0;
                        fread(&num, sizeof(double), 1, f);
                        fdefaults[j] = make_number_value(num);
                    } else if (tag == (uint8_t)VAL_BOOL) {
                        uint8_t b = 0;
                        fread(&b, sizeof(uint8_t), 1, f);
                        fdefaults[j] = make_bool_value(b != 0);
                    } else if (tag == (uint8_t)VAL_STRING) {
                        char *sv = read_str(f);
                        fdefaults[j] = make_string_value(sv ? sv : "");
                        free(sv);
                    } else {
                        fdefaults[j] = make_none_value();
                    }
                }
                chunk_add_struct(chunk, sname, fcount, fnames, fdefaults);
                for (int32_t j = 0; j < fcount; j++) free(fnames[j]);
                free(fnames);
                free(fdefaults);
                free(sname);
            }
        }

        /* Read source locations */
        uint32_t loc_count = 0;
        if (fread(&loc_count, sizeof(uint32_t), 1, f) == 1 && loc_count > 0) {
            free(chunk->locs);
            chunk->locs = malloc((size_t)loc_count * sizeof(BytecodeLoc));
            chunk->locs_cap = loc_count;
            chunk->locs_len = loc_count;
            for (uint32_t i = 0; i < loc_count; i++) {
                uint32_t coff = 0, fidx = 0;
                int32_t line = 0, col = 0;
                fread(&coff, sizeof(uint32_t), 1, f);
                fread(&fidx, sizeof(uint32_t), 1, f);
                fread(&line, sizeof(int32_t), 1, f);
                fread(&col, sizeof(int32_t), 1, f);
                chunk->locs[i].code_offset = coff;
                chunk->locs[i].file_str_idx = fidx;
                chunk->locs[i].line = line;
                chunk->locs[i].column = col;
            }
        }
    }

    fclose(f);
    return chunk;
}

int main(int argc, char **argv) {
    atexit(value_cleanup);
    atexit(module_system_cleanup);
    atexit(maybe_print_profile_at_exit);

    if (argc < 2) {
        repl_start();
        return 0;
    }

    int compile_mode = 0;
    int disassemble_mode = 0;
    int repl_mode = 0;
    int vm_mode = 0;
    const char *source_file = NULL;
    const char *output_file = NULL;

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
        } else if (strcmp(argv[i], "--vm") == 0) {
            vm_mode = 1;
        } else if (strcmp(argv[i], "--gc-stress") == 0) {
            gc_set_stress(1);
        } else if (strcmp(argv[i], "--profile") == 0) {
            gc_set_profile(1);
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

    char default_output[4096];
    if (compile_mode && !output_file) {
        strncpy(default_output, source_file, sizeof(default_output) - 5);
        default_output[sizeof(default_output) - 5] = '\0';
        char *dot = strrchr(default_output, '.');
        char *slash1 = strrchr(default_output, '/');
        char *slash2 = strrchr(default_output, '\\');
        char *slash = (slash1 > slash2) ? slash1 : slash2;
        if (dot && (!slash || dot > slash)) *dot = '\0';
        strcat(default_output, ".vbo");
        output_file = default_output;
    }

    char *source = read_file(source_file);
    module_system_init();
    module_set_current_file(source_file);
    lexer_init(source);
    StmtList program = parse_program();

    if (compile_mode || disassemble_mode || vm_mode) {
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
        } else if (vm_mode && !disassemble_mode) {
            VM *vm = vm_new(chunk);
            if (!vm) {
                ok = 0;
            } else {
                ok = (vm_execute(vm) == 0);
                vm_free(vm);
            }
        }

        codegen_free(gen);
        free_stmt_list(&program);
        free(source);
        return ok ? 0 : 1;
    } else {
        interpret_program(program);
    }

    free(source);
    return 0;
}