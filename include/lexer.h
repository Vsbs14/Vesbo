#ifndef VESBO_LEXER_H
#define VESBO_LEXER_H

#include "token.h"

typedef struct {
    const char *source;
    int start;
    int current;
    int line;
    int at_line_start;
    int indent_stack[128];
    int indent_count;
    int pending_dedents;
} LexerState;

void lexer_init(const char *source);
Token lexer_next_token(void);
void lexer_cleanup(void);

LexerState lexer_get_state(void);
void lexer_set_state(LexerState state);

#endif