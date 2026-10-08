#include "../include/repl.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/interpreter.h"
#include "../include/environment.h"
#include "../include/value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

#ifdef _WIN32
#include <io.h>
#define is_terminal() (_isatty(_fileno(stdin)))
#else
#include <unistd.h>
#define is_terminal() (isatty(fileno(stdin)))
#endif

#define VESBO_REPL_VERSION "0.2.0"
#define LINE_BUFFER_SIZE 4096

static void print_repl_help(void) {
    printf("\nVesbo Interactive REPL Help:\n");
    printf("  - Type any expression to evaluate and display its value:\n");
    printf("      >>> 40 + 2\n");
    printf("      >>> uppercase(#hello world#)\n");
    printf("      >>> [1, 2, 3][0]\n");
    printf("  - Declare and set variables:\n");
    printf("      >>> var count is 5\n");
    printf("      >>> set count to count + 1\n");
    printf("  - Multi-line blocks (functions, loops, conditionals):\n");
    printf("      Type 'ft', 'if', 'loop', or 'try' to start a block.\n");
    printf("      Indent the body lines, then press Enter on an empty line to execute.\n");
    printf("  - Special commands:\n");
    printf("      exit, quit   Exit the REPL\n");
    printf("      help         Show this help message\n\n");
}

static int is_empty_line(const char *str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) return 0;
        str++;
    }
    return 1;
}

static int starts_block(const char *str) {
    while (*str && isspace((unsigned char)*str)) str++;
    if (strncmp(str, "ft ", 3) == 0) return 1;
    if (strncmp(str, "if ", 3) == 0) return 1;
    if (strncmp(str, "else", 4) == 0) return 1;
    if (strncmp(str, "loop ", 5) == 0) return 1;
    if (strncmp(str, "try", 3) == 0) return 1;
    if (strncmp(str, "catch", 5) == 0) return 1;
    return 0;
}

void repl_start(void) {
    int interactive = is_terminal();

    if (interactive) {
        printf("Vesbo %s Interactive REPL\n", VESBO_REPL_VERSION);
        printf("Type code to evaluate. Type 'help' for examples or 'exit' to quit.\n\n");
    }

    Environment env;
    env_init(&env);

    /* Track all parsed statements so function bodies stay valid throughout session */
    StmtList *history = NULL;
    size_t history_count = 0;
    size_t history_cap = 0;

    char line[LINE_BUFFER_SIZE];
    char *accum = NULL;
    size_t accum_len = 0;
    size_t accum_cap = 0;
    int in_block = 0;

    while (1) {
        if (interactive) {
            if (in_block) {
                printf("..... ");
            } else {
                printf(">>> ");
            }
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), stdin)) {
            if (interactive) printf("\n");
            break;
        }

        /* Check for exit commands when not in a block */
        if (!in_block) {
            char trimmed[LINE_BUFFER_SIZE];
            size_t j = 0;
            for (size_t i = 0; line[i]; i++) {
                if (!isspace((unsigned char)line[i])) {
                    trimmed[j++] = line[i];
                }
            }
            trimmed[j] = '\0';

            if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
                break;
            }
            if (strcmp(trimmed, "help") == 0) {
                print_repl_help();
                continue;
            }
            if (j == 0) {
                continue;
            }
        }

        /* Multi-line block handling */
        if (!in_block && starts_block(line)) {
            in_block = 1;
            accum_len = 0;
            if (accum_cap < LINE_BUFFER_SIZE) {
                accum_cap = LINE_BUFFER_SIZE;
                accum = realloc(accum, accum_cap);
            }
            strcpy(accum, line);
            accum_len = strlen(accum);
            continue;
        }

        if (in_block) {
            if (is_empty_line(line)) {
                /* End of multi-line block */
                in_block = 0;
            } else {
                size_t line_len = strlen(line);
                if (accum_len + line_len + 1 > accum_cap) {
                    accum_cap = (accum_len + line_len + 1) * 2;
                    accum = realloc(accum, accum_cap);
                }
                strcat(accum, line);
                accum_len += line_len;
                continue;
            }
        }

        const char *source_to_run = accum_len > 0 ? accum : line;

        /* Parse and execute with error recovery */
        jmp_buf err_buf;
        if (setjmp(err_buf) == 0) {
            parser_set_repl_mode(&err_buf);
            lexer_init(source_to_run);
            StmtList program = parse_program();
            parser_set_repl_mode(NULL);

            if (program.count > 0) {
                interpret_repl_program(program, &env);

                /* Retain AST in history so registered functions remain valid */
                if (history_count >= history_cap) {
                    history_cap = history_cap == 0 ? 16 : history_cap * 2;
                    history = realloc(history, sizeof(StmtList) * history_cap);
                }
                history[history_count++] = program;
            }
        } else {
            /* Recovered from parse error */
            parser_set_repl_mode(NULL);
        }

        if (accum) {
            accum[0] = '\0';
            accum_len = 0;
        }
    }

    /* Cleanup */
    free(accum);
    for (size_t i = 0; i < history_count; i++) {
        free_stmt_list(&history[i]);
    }
    free(history);

    env_free(&env);
    interpret_cleanup();
    value_cleanup();
}
