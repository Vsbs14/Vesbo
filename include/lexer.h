#ifndef VESBO_LEXER_H
#define VESBO_LEXER_H

#include "token.h"

void lexer_init(const char *source);
Token lexer_next_token(void);

#endif