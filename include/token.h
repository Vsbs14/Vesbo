#ifndef VESBO_TOKEN_H
#define VESBO_TOKEN_H

typedef enum {
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_IDENT,

    TOKEN_VAR, TOKEN_IS, TOKEN_SET, TOKEN_TO, TOKEN_FT, TOKEN_GIVES,
    TOKEN_RETURN, TOKEN_IF, TOKEN_ELSE, TOKEN_LOOP, TOKEN_TILL, TOKEN_FROM,
    TOKEN_THROUGH, TOKEN_AS, TOKEN_TRY, TOKEN_CATCH, TOKEN_OUTPUT,
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NONE, TOKEN_AND, TOKEN_OR, TOKEN_NOT,

    TOKEN_EQUALS,
    TOKEN_DOES_NOT_EQUAL,
    TOKEN_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_LESS_THAN_OR_EQUAL,
    TOKEN_GREATER_THAN_OR_EQUAL,

    // Symbols
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    TOKEN_COMMA,        // ,
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /

    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    double number_val;
    int line;
} Token;

#endif