#include "../include/lexer.h"
#include "../include/token.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct {
    const char *source;
    int start;
    int current;
    int line;
    int at_line_start;
    int indent_stack[128];
    int indent_count;
    int pending_dedents;
} Lexer;

static Lexer lexer;
static char **token_strings = NULL;
static int token_strings_count = 0;
static int token_strings_cap = 0;

static void track_token_string(char *s) {
    if (!s) return;
    if (token_strings_count >= token_strings_cap) {
        token_strings_cap = token_strings_cap == 0 ? 64 : token_strings_cap * 2;
        token_strings = realloc(token_strings, sizeof(char *) * token_strings_cap);
    }
    token_strings[token_strings_count++] = s;
}

void lexer_cleanup(void) {
    for (int i = 0; i < token_strings_count; i++) {
        free(token_strings[i]);
    }
    free(token_strings);
    token_strings = NULL;
    token_strings_count = 0;
    token_strings_cap = 0;
}

void lexer_init(const char *source) {
    lexer_cleanup();
    lexer.source = source;
    lexer.start = 0;
    lexer.current = 0;
    lexer.line = 1;
    lexer.at_line_start = 1;
    lexer.indent_stack[0] = 0;
    lexer.indent_count = 1;
    lexer.pending_dedents = 0;
}

static char peek() { return lexer.source[lexer.current]; }
static char advance() { return lexer.source[lexer.current++]; }
static int is_at_end() { return peek() == '\0'; }

static Token make_token(TokenType type) {
    Token t;
    t.type = type;
    t.line = lexer.line;
    int len = lexer.current - lexer.start;
    t.lexeme = malloc(len + 1);
    if (t.lexeme) {
        strncpy(t.lexeme, &lexer.source[lexer.start], len);
        t.lexeme[len] = '\0';
        track_token_string(t.lexeme);
    }
    return t;
}

static Token layout_token(TokenType type) {
    lexer.start = lexer.current;
    return make_token(type);
}

static int handle_line_start(void) {
    int indentation = 0;
    while (peek() == ' ' || peek() == '\t' || peek() == '\r') {
        indentation += peek() == '\t' ? 4 : 1;
        advance();
    }

    if (peek() == '\n' || (peek() == '-' && lexer.source[lexer.current + 1] == '-')) {
        while (peek() != '\n' && !is_at_end()) advance();
        if (peek() == '\n') {
            advance();
            lexer.line++;
        }
        lexer.start = lexer.current;
        return 1;
    }

    int current_indent = lexer.indent_stack[lexer.indent_count - 1];
    if (indentation > current_indent) {
        if (lexer.indent_count >= 128) return -1;
        lexer.indent_stack[lexer.indent_count++] = indentation;
        lexer.at_line_start = 0;
        return 2;
    }
    if (indentation < current_indent) {
        int emitted_first = 0;
        while (lexer.indent_count > 1 &&
               indentation < lexer.indent_stack[lexer.indent_count - 1]) {
            lexer.indent_count--;
            if (emitted_first) lexer.pending_dedents++;
            emitted_first = 1;
        }
        if (indentation != lexer.indent_stack[lexer.indent_count - 1]) return -1;
        lexer.at_line_start = 0;
        return 3;
    }

    lexer.at_line_start = 0;
    return 0;
}

static Token string_token() {
    // lexer.start currently points at the opening '#' (set by the
    // caller before advance() consumed it) -- move it past that
    // delimiter so the lexeme only contains the string's contents.
    lexer.start = lexer.current;

    while (peek() != '#' && !is_at_end()) {
        if (peek() == '\n') lexer.line++;
        advance();
    }
    Token t = make_token(TOKEN_STRING);
    if (!is_at_end()) advance(); // consume closing #
    return t;
}

static Token number_token() {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(lexer.source[lexer.current + 1])) {
        advance();
        while (isdigit(peek())) advance();
    }
    Token t = make_token(TOKEN_NUMBER);
    t.number_val = strtod(&lexer.source[lexer.start], NULL);
    return t;
}

static int match_phrase_word(const char *expected) {
    int checkpoint_pos = lexer.current;
    int checkpoint_line = lexer.line;

    while (peek() == ' ' || peek() == '\t' || peek() == '\r') {
        advance();
    }

    int word_start = lexer.current;
    while (isalnum(peek()) || peek() == '_') {
        advance();
    }
    int word_len = lexer.current - word_start;
    int expected_len = (int)strlen(expected);

    if (word_len == expected_len &&
        strncmp(&lexer.source[word_start], expected, expected_len) == 0) {
        return 1;
    }

    lexer.current = checkpoint_pos;
    lexer.line = checkpoint_line;
    return 0;
}

static Token handle_is_phrases() {
    int checkpoint_pos = lexer.current;
    int checkpoint_line = lexer.line;

    if (match_phrase_word("less")) {
        if (match_phrase_word("than")) {
            int before_or = lexer.current;
            int before_or_line = lexer.line;
            if (match_phrase_word("or")) {
                if (match_phrase_word("equal")) {
                    return make_token(TOKEN_LESS_THAN_OR_EQUAL);
                }
                lexer.current = before_or;
                lexer.line = before_or_line;
            }
            return make_token(TOKEN_LESS_THAN);
        }
        // "less" matched but "than" didn't -- roll back the whole phrase
        lexer.current = checkpoint_pos;
        lexer.line = checkpoint_line;
        return make_token(TOKEN_IS);
    }

    if (match_phrase_word("greater")) {
        if (match_phrase_word("than")) {
            int before_or = lexer.current;
            int before_or_line = lexer.line;
            if (match_phrase_word("or")) {
                if (match_phrase_word("equal")) {
                    return make_token(TOKEN_GREATER_THAN_OR_EQUAL);
                }
                lexer.current = before_or;
                lexer.line = before_or_line;
            }
            return make_token(TOKEN_GREATER_THAN);
        }
        lexer.current = checkpoint_pos;
        lexer.line = checkpoint_line;
        return make_token(TOKEN_IS);
    }

    return make_token(TOKEN_IS);
}

static Token handle_does_phrase() {
    int checkpoint_pos = lexer.current;
    int checkpoint_line = lexer.line;

    if (match_phrase_word("not")) {
        if (match_phrase_word("equal")) {
            return make_token(TOKEN_DOES_NOT_EQUAL);
        }
    }

    lexer.current = checkpoint_pos;
    lexer.line = checkpoint_line;
    return make_token(TOKEN_IDENT);
}

static Token identifier_or_keyword_token() {
    while (isalnum(peek()) || peek() == '_') advance();
    int len = lexer.current - lexer.start;
    const char *text = &lexer.source[lexer.start];

    // Multi-word phrase entry points
    if (len == 2 && strncmp(text, "is", 2) == 0) return handle_is_phrases();
    if (len == 4 && strncmp(text, "does", 4) == 0) return handle_does_phrase();

    // Single-word keywords
    if (len == 3 && strncmp(text, "var", 3) == 0) return make_token(TOKEN_VAR);
    if (len == 3 && strncmp(text, "set", 3) == 0) return make_token(TOKEN_SET);
    if (len == 2 && strncmp(text, "to", 2) == 0) return make_token(TOKEN_TO);
    if (len == 2 && strncmp(text, "ft", 2) == 0) return make_token(TOKEN_FT);
    if (len == 5 && strncmp(text, "gives", 5) == 0) return make_token(TOKEN_GIVES);
    if (len == 6 && strncmp(text, "return", 6) == 0) return make_token(TOKEN_RETURN);
    if (len == 2 && strncmp(text, "if", 2) == 0) return make_token(TOKEN_IF);
    if (len == 4 && strncmp(text, "else", 4) == 0) return make_token(TOKEN_ELSE);
    if (len == 4 && strncmp(text, "loop", 4) == 0) return make_token(TOKEN_LOOP);
    if (len == 4 && strncmp(text, "till", 4) == 0) return make_token(TOKEN_TILL);
    if (len == 4 && strncmp(text, "from", 4) == 0) return make_token(TOKEN_FROM);
    if (len == 7 && strncmp(text, "through", 7) == 0) return make_token(TOKEN_THROUGH);
    if (len == 2 && strncmp(text, "as", 2) == 0) return make_token(TOKEN_AS);
    if (len == 3 && strncmp(text, "try", 3) == 0) return make_token(TOKEN_TRY);
    if (len == 5 && strncmp(text, "catch", 5) == 0) return make_token(TOKEN_CATCH);
    if (len == 6 && strncmp(text, "output", 6) == 0) return make_token(TOKEN_OUTPUT);
    if (len == 4 && strncmp(text, "true", 4) == 0) return make_token(TOKEN_TRUE);
    if (len == 5 && strncmp(text, "false", 5) == 0) return make_token(TOKEN_FALSE);
    if (len == 4 && strncmp(text, "none", 4) == 0) return make_token(TOKEN_NONE);
    if (len == 6 && strncmp(text, "equals", 6) == 0) return make_token(TOKEN_EQUALS);
    if (len == 3 && strncmp(text, "AND", 3) == 0) return make_token(TOKEN_AND);
    if (len == 2 && strncmp(text, "OR", 2) == 0) return make_token(TOKEN_OR);
    if (len == 3 && strncmp(text, "NOT", 3) == 0) return make_token(TOKEN_NOT);

    return make_token(TOKEN_IDENT);
}

Token lexer_next_token() {
    for (;;) {
        if (lexer.pending_dedents > 0) {
            lexer.pending_dedents--;
            return layout_token(TOKEN_DEDENT);
        }

        if (lexer.at_line_start) {
            int line_state = handle_line_start();
            if (line_state == 1) continue;
            if (line_state == 2) return layout_token(TOKEN_INDENT);
            if (line_state == 3) return layout_token(TOKEN_DEDENT);
            if (line_state == -1) return layout_token(TOKEN_ERROR);
        }

        while (peek() == ' ' || peek() == '\t' || peek() == '\r') advance();
        if (peek() == '-' && lexer.source[lexer.current + 1] == '-') {
            while (peek() != '\n' && !is_at_end()) advance();
            continue;
        }
        break;
    }

    lexer.start = lexer.current;

    if (is_at_end()) {
        if (lexer.indent_count > 1) {
            lexer.indent_count--;
            return layout_token(TOKEN_DEDENT);
        }
        return make_token(TOKEN_EOF);
    }

    char c = advance();

    if (c == '\n') {
        lexer.line++;
        lexer.at_line_start = 1;
        return make_token(TOKEN_NEWLINE);
    }

    if (isdigit(c)) return number_token();
    if (isalpha(c) || c == '_') return identifier_or_keyword_token();
    if (c == '#') return string_token();

    switch (c) {
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case ',': return make_token(TOKEN_COMMA);
        case '+': return make_token(TOKEN_PLUS);
        case '-': return make_token(TOKEN_MINUS);
        case '*': return make_token(TOKEN_STAR);
        case '/': return make_token(TOKEN_SLASH);
        case '%': return make_token(TOKEN_PERCENT);
    }

    return make_token(TOKEN_ERROR);
}