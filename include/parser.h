#ifndef VESBO_PARSER_H
#define VESBO_PARSER_H

#include "ast.h"
#include <setjmp.h>

// Parses an entire program (already-lexed via lexer_init) into a
// list of top-level statements (typically function declarations).
// Calls lexer_next_token() internally as needed.
StmtList parse_program(void);

// Set or clear REPL mode jmp_buf for error recovery without exiting
void parser_set_repl_mode(jmp_buf *buf);

#endif