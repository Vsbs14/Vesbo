#include <stdio.h>
#include <stdlib.h>
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/interpreter.h"
#include "../include/value.h"

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

int main(int argc, char **argv) {
    atexit(value_cleanup);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.vsb>\n", argv[0]);
        return 1;
    }

    char *source = read_file(argv[1]);
    lexer_init(source);
    StmtList program = parse_program();
    interpret_program(program);

    free(source);
    return 0;
}