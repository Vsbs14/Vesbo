#include "../include/module.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



static char **imported_modules = NULL;
static int imported_count = 0;
static int imported_cap = 0;

static char current_file_path[1024] = {0};

void module_system_init(void) {
    module_system_cleanup();
}

void module_system_cleanup(void) {
    if (imported_modules) {
        for (int i = 0; i < imported_count; i++) {
            free(imported_modules[i]);
        }
        free(imported_modules);
        imported_modules = NULL;
    }
    imported_count = 0;
    imported_cap = 0;
}

void module_set_current_file(const char *filepath) {
    if (filepath) {
        strncpy(current_file_path, filepath, sizeof(current_file_path) - 1);
        current_file_path[sizeof(current_file_path) - 1] = '\0';
    } else {
        current_file_path[0] = '\0';
    }
}

const char *module_get_current_file(void) {
    return current_file_path[0] ? current_file_path : NULL;
}

static char *get_canonical_path(const char *path) {
    char resolved[1024];
#ifdef _WIN32
    if (!_fullpath(resolved, path, sizeof(resolved))) return NULL;
    for (char *p = resolved; *p; p++) {
        if (*p == '/') *p = '\\';
        *p = (char)tolower((unsigned char)*p);
    }
#else
    if (!realpath(path, resolved)) return NULL;
#endif
    size_t len = strlen(resolved);
    char *copy = malloc(len + 1);
    if (copy) strcpy(copy, resolved);
    return copy;
}

static int file_exists_at(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

char *module_resolve_path(const char *raw_path, const char *relative_to_file) {
    if (!raw_path || raw_path[0] == '\0') return NULL;

    char candidate[4096];
    size_t raw_len = strlen(raw_path);
    int has_vsb_ext = (raw_len >= 4 && strcmp(raw_path + raw_len - 4, ".vsb") == 0);

    // 1. Try relative to relative_to_file's directory
    if (relative_to_file && relative_to_file[0] != '\0') {
        const char *last_slash1 = strrchr(relative_to_file, '/');
        const char *last_slash2 = strrchr(relative_to_file, '\\');
        const char *last_slash = (last_slash1 > last_slash2) ? last_slash1 : last_slash2;

        if (last_slash) {
            size_t dir_len = (size_t)(last_slash - relative_to_file);
            char dir[1024];
            if (dir_len < sizeof(dir)) {
                strncpy(dir, relative_to_file, dir_len);
                dir[dir_len] = '\0';

                // Try dir/raw_path
                snprintf(candidate, sizeof(candidate), "%s/%s", dir, raw_path);
                if (file_exists_at(candidate)) return get_canonical_path(candidate);

                // Try dir/raw_path.vsb
                if (!has_vsb_ext) {
                    snprintf(candidate, sizeof(candidate), "%s/%s.vsb", dir, raw_path);
                    if (file_exists_at(candidate)) return get_canonical_path(candidate);
                }
            }
        }
    }

    // 2. Try relative to current working directory
    if (file_exists_at(raw_path)) return get_canonical_path(raw_path);

    if (!has_vsb_ext) {
        snprintf(candidate, sizeof(candidate), "%s.vsb", raw_path);
        if (file_exists_at(candidate)) return get_canonical_path(candidate);
    }

    return NULL;
}

int module_is_imported(const char *canonical_path) {
    if (!canonical_path) return 0;
    for (int i = 0; i < imported_count; i++) {
        if (strcmp(imported_modules[i], canonical_path) == 0) {
            return 1;
        }
    }
    return 0;
}

void module_mark_imported(const char *canonical_path) {
    if (!canonical_path || module_is_imported(canonical_path)) return;
    if (imported_count >= imported_cap) {
        imported_cap = imported_cap == 0 ? 8 : imported_cap * 2;
        imported_modules = realloc(imported_modules, sizeof(char *) * imported_cap);
    }
    size_t len = strlen(canonical_path);
    char *copy = malloc(len + 1);
    if (copy) strcpy(copy, canonical_path);
    imported_modules[imported_count++] = copy;
}

int module_parse_file(const char *canonical_path, StmtList *out_ast, char **out_error) {
    FILE *f = fopen(canonical_path, "rb");
    if (!f) {
        if (out_error) {
            char msg[256];
            snprintf(msg, sizeof(msg), "cannot open file '%s'", canonical_path);
            size_t mlen = strlen(msg);
            *out_error = malloc(mlen + 1);
            if (*out_error) strcpy(*out_error, msg);
        }
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        if (out_error) {
            const char *msg = "cannot determine file size";
            *out_error = malloc(strlen(msg) + 1);
            if (*out_error) strcpy(*out_error, msg);
        }
        return 0;
    }

    char *source = malloc(size + 1);
    if (!source) {
        fclose(f);
        if (out_error) {
            const char *msg = "out of memory reading module";
            *out_error = malloc(strlen(msg) + 1);
            if (*out_error) strcpy(*out_error, msg);
        }
        return 0;
    }
    size_t bytes_read = fread(source, 1, (size_t)size, f);
    source[bytes_read] = '\0';
    fclose(f);

    LexerState saved_lex = lexer_get_state();
    ParserState saved_parse = parser_get_state();

    lexer_init(source);
    *out_ast = parse_program();

    lexer_set_state(saved_lex);
    parser_set_state(saved_parse);

    free(source);
    return 1;
}
