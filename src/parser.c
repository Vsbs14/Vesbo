#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static Token current_tok;
static Token previous_tok;
static jmp_buf *repl_recovery_buf = NULL;

void parser_set_repl_mode(jmp_buf *buf) {
    repl_recovery_buf = buf;
}

ParserState parser_get_state(void) {
    ParserState s;
    s.current_tok = current_tok;
    s.previous_tok = previous_tok;
    s.repl_recovery_buf = repl_recovery_buf;
    return s;
}

void parser_set_state(ParserState state) {
    current_tok = state.current_tok;
    previous_tok = state.previous_tok;
    repl_recovery_buf = state.repl_recovery_buf;
}

static void parser_advance(void) {
    previous_tok = current_tok;
    current_tok = lexer_next_token();
}

static int check(TokenType type) {
    return current_tok.type == type;
}

static int match(TokenType type) {
    if (!check(type)) return 0;
    parser_advance();
    return 1;
}

static void error_at(Token *tok, const char *message) {
    fprintf(stderr, "Parse error at line %d (near '%s'): %s\n",
            tok->line, tok->lexeme ? tok->lexeme : "?", message);
    lexer_cleanup();
    if (repl_recovery_buf) {
        longjmp(*repl_recovery_buf, 1);
    }
    exit(1);
}

static Token consume(TokenType type, const char *message) {
    if (check(type)) {
        Token t = current_tok;
        parser_advance();
        return t;
    }
    error_at(&current_tok, message);
    Token dummy = {0};
    return dummy;
}

static Expr *parse_expression(void);
static StmtList parse_block(void);
static Stmt *parse_statement(void);

static Expr *parse_or(void);

static Expr *parse_postfix(Expr *expr, int line) {
    while (match(TOKEN_LBRACKET)) {
        Expr *index = parse_expression();
        consume(TOKEN_RBRACKET, "expected ']' after index");
        expr = make_index_expr(expr, index, line);
    }
    return expr;
}

static Expr *parse_sub_expression(const char *expr_src, int line) {
    (void)line;
    LexerState saved_lex = lexer_get_state();
    ParserState saved_parse = parser_get_state();

    lexer_init(expr_src);
    parser_advance();
    Expr *expr = parse_expression();
    if (current_tok.type != TOKEN_EOF) {
        error_at(&current_tok, "unexpected token in interpolated expression");
    }

    lexer_set_state(saved_lex);
    parser_set_state(saved_parse);

    return expr;
}

static Expr *parse_interpolated_string(const char *raw, int line) {
    int cap = 64;
    int text_len = 0;
    char *text_buf = malloc(cap);
    Expr *result = NULL;

    const char *p = raw;
    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            if (text_len + 1 >= cap) { cap *= 2; text_buf = realloc(text_buf, cap); }
            text_buf[text_len++] = '{';
            p += 2;
        } else if (p[0] == '}' && p[1] == '}') {
            if (text_len + 1 >= cap) { cap *= 2; text_buf = realloc(text_buf, cap); }
            text_buf[text_len++] = '}';
            p += 2;
        } else if (p[0] == '\\' && p[1] != '\0') {
            char esc = p[1];
            switch (esc) {
                case 'n': esc = '\n'; break;
                case 't': esc = '\t'; break;
                case 'r': esc = '\r'; break;
                case '\\': esc = '\\'; break;
                case '"': esc = '"'; break;
                case '\'': esc = '\''; break;
                case '#': esc = '#'; break;
                default: break;
            }
            if (text_len + 1 >= cap) { cap *= 2; text_buf = realloc(text_buf, cap); }
            text_buf[text_len++] = esc;
            p += 2;
        } else if (p[0] == '{') {
            if (text_len > 0) {
                text_buf[text_len] = '\0';
                Expr *lit = make_string_expr(text_buf, line);
                if (!result) {
                    result = lit;
                } else {
                    result = make_binary_expr(BIN_ADD, result, lit, line);
                }
                text_len = 0;
            }

            p++; // skip '{'
            int depth = 1;
            const char *expr_start = p;
            while (*p && depth > 0) {
                if (*p == '"' || *p == '\'' || *p == '#') {
                    char q = *p++;
                    while (*p && *p != q) {
                        if (*p == '\\' && *(p + 1)) p++;
                        p++;
                    }
                    if (*p == q) p++;
                } else if (*p == '{') {
                    depth++;
                    p++;
                } else if (*p == '}') {
                    depth--;
                    if (depth == 0) break;
                    p++;
                } else {
                    p++;
                }
            }

            if (depth > 0) {
                fprintf(stderr, "Parse error at line %d: unterminated expression in interpolated string\n", line);
                free(text_buf);
                if (repl_recovery_buf) longjmp(*repl_recovery_buf, 1);
                exit(1);
            }

            int expr_len = (int)(p - expr_start);
            if (*p == '}') p++; // skip closing '}'

            char *expr_code = malloc(expr_len + 1);
            memcpy(expr_code, expr_start, expr_len);
            expr_code[expr_len] = '\0';

            const char *check_p = expr_code;
            while (*check_p && isspace((unsigned char)*check_p)) check_p++;
            if (*check_p == '\0') {
                fprintf(stderr, "Parse error at line %d: empty expression '{}' in interpolated string\n", line);
                free(expr_code);
                free(text_buf);
                if (repl_recovery_buf) longjmp(*repl_recovery_buf, 1);
                exit(1);
            }

            Expr *sub_expr = parse_sub_expression(expr_code, line);
            free(expr_code);

            ExprList args;
            expr_list_init(&args);
            expr_list_add(&args, sub_expr);
            Expr *str_call = make_call_expr("string", args, line);

            if (!result) {
                result = str_call;
            } else {
                result = make_binary_expr(BIN_ADD, result, str_call, line);
            }
        } else if (p[0] == '}') {
            fprintf(stderr, "Parse error at line %d: unexpected '}' in interpolated string (use '}}' to escape)\n", line);
            free(text_buf);
            if (repl_recovery_buf) longjmp(*repl_recovery_buf, 1);
            exit(1);
        } else {
            if (text_len + 1 >= cap) { cap *= 2; text_buf = realloc(text_buf, cap); }
            text_buf[text_len++] = *p++;
        }
    }

    if (text_len > 0) {
        text_buf[text_len] = '\0';
        Expr *lit = make_string_expr(text_buf, line);
        if (!result) {
            result = lit;
        } else {
            result = make_binary_expr(BIN_ADD, result, lit, line);
        }
    }

    free(text_buf);

    if (!result) {
        result = make_string_expr("", line);
    }

    return result;
}

static Expr *parse_primary(void) {
    int line = current_tok.line;

    if (match(TOKEN_NUMBER)) {
        return make_number_expr(previous_tok.number_val, line);
    }
    if (match(TOKEN_STRING)) {
        return make_string_expr(previous_tok.lexeme, line);
    }
    if (match(TOKEN_INTERPOLATED_STRING)) {
        return parse_postfix(parse_interpolated_string(previous_tok.lexeme, line), line);
    }
    if (match(TOKEN_TRUE)) {
        return make_bool_expr(1, line);
    }
    if (match(TOKEN_FALSE)) {
        return make_bool_expr(0, line);
    }
    if (match(TOKEN_NONE)) {
        return make_none_expr(line);
    }
    if (match(TOKEN_LPAREN)) {
        Expr *e = parse_expression();
        consume(TOKEN_RPAREN, "expected ')' after expression");
        return parse_postfix(e, line);
    }
    if (match(TOKEN_LBRACKET)) {
        ExprList elements;
        expr_list_init(&elements);
        if (!check(TOKEN_RBRACKET)) {
            do {
                expr_list_add(&elements, parse_expression());
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RBRACKET, "expected ']' after array elements");
        return parse_postfix(make_array_expr(elements, line), line);
    }
    if (match(TOKEN_LBRACE)) {
        MapEntryList entries;
        map_entry_list_init(&entries);
        if (!check(TOKEN_RBRACE)) {
            do {
                Expr *key = parse_expression();
                consume(TOKEN_COLON, "expected ':' after map key");
                Expr *val = parse_expression();
                map_entry_list_add(&entries, key, val);
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RBRACE, "expected '}' after map entries");
        return parse_postfix(make_map_expr(entries, line), line);
    }
    if (match(TOKEN_IDENT)) {
        char name[256];
        strncpy(name, previous_tok.lexeme, 255);
        name[255] = '\0';

        Expr *expr;

        if (match(TOKEN_LPAREN)) {
            ExprList args;
            expr_list_init(&args);
            if (!check(TOKEN_RPAREN)) {
                do {
                    expr_list_add(&args, parse_expression());
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "expected ')' after arguments");
            expr = make_call_expr(name, args, line);
        } else {
            expr = make_ident_expr(name, line);
        }

        return parse_postfix(expr, line);
    }

    error_at(&current_tok, "expected an expression");
    return NULL; // unreachable
}

static Expr *parse_unary(void) {
    int line = current_tok.line;
    if (match(TOKEN_NOT)) {
        Expr *operand = parse_unary();
        return make_unary_expr(UN_NOT, operand, line);
    }
    if (match(TOKEN_MINUS)) {
        Expr *operand = parse_unary();
        return make_unary_expr(UN_NEGATE, operand, line);
    }
    return parse_primary();
}

static Expr *parse_multiplicative(void) {
    Expr *left = parse_unary();
    for (;;) {
        int line = current_tok.line;
        if (match(TOKEN_STAR)) {
            Expr *right = parse_unary();
            left = make_binary_expr(BIN_MUL, left, right, line);
        } else if (match(TOKEN_SLASH)) {
            Expr *right = parse_unary();
            left = make_binary_expr(BIN_DIV, left, right, line);
        } else if (match(TOKEN_PERCENT)) {
            Expr *right = parse_unary();
            left = make_binary_expr(BIN_MOD, left, right, line);
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_additive(void) {
    Expr *left = parse_multiplicative();
    for (;;) {
        int line = current_tok.line;
        if (match(TOKEN_PLUS)) {
            Expr *right = parse_multiplicative();
            left = make_binary_expr(BIN_ADD, left, right, line);
        } else if (match(TOKEN_MINUS)) {
            Expr *right = parse_multiplicative();
            left = make_binary_expr(BIN_SUB, left, right, line);
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_comparison(void) {
    Expr *left = parse_additive();
    for (;;) {
        int line = current_tok.line;
        if (match(TOKEN_LESS_THAN)) {
            left = make_binary_expr(BIN_LESS, left, parse_additive(), line);
        } else if (match(TOKEN_GREATER_THAN)) {
            left = make_binary_expr(BIN_GREATER, left, parse_additive(), line);
        } else if (match(TOKEN_LESS_THAN_OR_EQUAL)) {
            left = make_binary_expr(BIN_LESS_EQUAL, left, parse_additive(), line);
        } else if (match(TOKEN_GREATER_THAN_OR_EQUAL)) {
            left = make_binary_expr(BIN_GREATER_EQUAL, left, parse_additive(), line);
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_equality(void) {
    Expr *left = parse_comparison();
    for (;;) {
        int line = current_tok.line;
        if (match(TOKEN_EQUALS)) {
            left = make_binary_expr(BIN_EQUALS, left, parse_comparison(), line);
        } else if (match(TOKEN_DOES_NOT_EQUAL)) {
            left = make_binary_expr(BIN_NOT_EQUALS, left, parse_comparison(), line);
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_and(void) {
    Expr *left = parse_equality();
    while (match(TOKEN_AND)) {
        int line = previous_tok.line;
        Expr *right = parse_equality();
        left = make_binary_expr(BIN_AND, left, right, line);
    }
    return left;
}

static Expr *parse_or(void) {
    Expr *left = parse_and();
    while (match(TOKEN_OR)) {
        int line = previous_tok.line;
        Expr *right = parse_and();
        left = make_binary_expr(BIN_OR, left, right, line);
    }
    return left;
}

static Expr *parse_expression(void) {
    return parse_or();
}

static StmtList parse_block(void) {
    consume(TOKEN_NEWLINE, "expected a new line before block");
    consume(TOKEN_INDENT, "expected an indented block");
    StmtList stmts;
    stmt_list_init(&stmts);
    while (!check(TOKEN_DEDENT) && !check(TOKEN_EOF)) {
        if (match(TOKEN_NEWLINE)) continue;
        stmt_list_add(&stmts, parse_statement());
    }
    consume(TOKEN_DEDENT, "expected block indentation to end");
    return stmts;
}

static Stmt *parse_var_decl(int is_global) {
    int line = current_tok.line;
    consume(TOKEN_VAR, "expected 'var'");
    Token name_tok = consume(TOKEN_IDENT, "expected variable name");
    consume(TOKEN_IS, "expected 'is' after variable name");
    Expr *value = parse_expression();
    return make_var_decl_stmt(name_tok.lexeme, value, is_global, line);
}

static Stmt *parse_set(void) {
    int line = current_tok.line;
    consume(TOKEN_SET, "expected 'set'");
    Token name_tok = consume(TOKEN_IDENT, "expected variable name");
    if (match(TOKEN_LBRACKET)) {
        Expr *index = parse_expression();
        consume(TOKEN_RBRACKET, "expected ']' after index");
        consume(TOKEN_TO, "expected 'to' after index in 'set' statement");
        Expr *value = parse_expression();
        return make_index_set_stmt(name_tok.lexeme, index, value, line);
    }
    consume(TOKEN_TO, "expected 'to' after variable name in 'set' statement");
    Expr *value = parse_expression();
    return make_assign_stmt(name_tok.lexeme, value, line);
}

static Stmt *parse_if(void) {
    int line = current_tok.line;
    consume(TOKEN_IF, "expected 'if'");
    Expr *condition = parse_expression();
    StmtList then_branch = parse_block();

    StmtList else_branch;
    stmt_list_init(&else_branch);
    int has_else = 0;

    if (match(TOKEN_ELSE)) {
        has_else = 1;
        if (check(TOKEN_IF)) {
            Stmt *nested = parse_if();
            stmt_list_add(&else_branch, nested);
        } else {
            else_branch = parse_block();
        }
    }

    return make_if_stmt(condition, then_branch, else_branch, has_else, line);
}

static Stmt *parse_loop(void) {
    int line = current_tok.line;
    consume(TOKEN_LOOP, "expected 'loop'");

    if (match(TOKEN_TILL)) {
        Expr *condition = parse_expression();
        StmtList body = parse_block();
        return make_loop_till_stmt(condition, body, line);
    }

    if (match(TOKEN_WHILE)) {
        Expr *condition = parse_expression();
        StmtList body = parse_block();
        return make_loop_while_stmt(condition, body, line);
    }

    if (match(TOKEN_FROM)) {
        Expr *from = parse_expression();
        consume(TOKEN_TO, "expected 'to' in 'loop from X to Y'");
        Expr *to = parse_expression();
        consume(TOKEN_AS, "expected 'as' in 'loop from X to Y as name'");
        Token var_tok = consume(TOKEN_IDENT, "expected loop variable name after 'as'");
        StmtList body = parse_block();
        return make_loop_range_stmt(from, to, var_tok.lexeme, body, line);
    }

    if (match(TOKEN_THROUGH)) {
        Expr *collection = parse_expression();
        consume(TOKEN_AS, "expected 'as' in 'loop through X as name'");
        Token item_tok = consume(TOKEN_IDENT, "expected item name after 'as'");
        StmtList body = parse_block();
        return make_loop_through_stmt(collection, item_tok.lexeme, body, line);
    }

    error_at(&current_tok, "expected 'while', 'till', 'from', or 'through' after 'loop'");
    return NULL; // unreachable
}

static Stmt *parse_func_decl(void) {
    int line = current_tok.line;
    consume(TOKEN_FT, "expected 'fnc' or 'ft'");
    Token name_tok = consume(TOKEN_IDENT, "expected function name");
    consume(TOKEN_LPAREN, "expected '(' after function name");

    StringList params;
    string_list_init(&params);
    if (!check(TOKEN_RPAREN)) {
        do {
            Token p = consume(TOKEN_IDENT, "expected parameter name");
            string_list_add(&params, p.lexeme);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "expected ')' after parameters");

    if (match(TOKEN_GIVES)) {
        consume(TOKEN_IDENT, "expected a type name after 'gives'");
    }

    StmtList body = parse_block();
    return make_func_decl_stmt(name_tok.lexeme, params, body, line);
}

static Stmt *parse_return(void) {
    int line = current_tok.line;
    consume(TOKEN_RETURN, "expected 'return'");
    if (check(TOKEN_NEWLINE) || check(TOKEN_DEDENT)) {
        return make_return_stmt(NULL, line);
    }
    Expr *value = parse_expression();
    return make_return_stmt(value, line);
}

static Stmt *parse_output(void) {
    int line = current_tok.line;
    consume(TOKEN_OUTPUT, "expected 'output'");
    Expr *value = parse_expression();
    return make_output_stmt(value, line);
}

static Stmt *parse_try_catch(void) {
    int line = current_tok.line;
    consume(TOKEN_TRY, "expected 'try'");
    StmtList try_body = parse_block();
    consume(TOKEN_CATCH, "expected 'catch' after try block");
    Token error_tok = consume(TOKEN_IDENT, "expected error variable name after 'catch'");
    StmtList catch_body = parse_block();
    return make_try_catch_stmt(try_body, error_tok.lexeme, catch_body, line);
}

static Stmt *parse_import(void) {
    int line = current_tok.line;
    consume(TOKEN_IMPORT, "expected 'import'");
    char *path = NULL;
    if (check(TOKEN_STRING)) {
        Token tok = consume(TOKEN_STRING, "expected module path string");
        size_t len = strlen(tok.lexeme);
        path = malloc(len + 1);
        if (path) strcpy(path, tok.lexeme);
    } else if (check(TOKEN_IDENT)) {
        Token tok = consume(TOKEN_IDENT, "expected module name");
        size_t len = strlen(tok.lexeme);
        path = malloc(len + 1);
        if (path) strcpy(path, tok.lexeme);
    } else {
        error_at(&current_tok, "expected module name or file path after 'import'");
        return NULL;
    }
    return make_import_stmt(path, line);
}

static Stmt *parse_statement(void) {
    if (check(TOKEN_IMPORT)) return parse_import();
    if (check(TOKEN_VAR)) return parse_var_decl(0);
    if (check(TOKEN_SET)) return parse_set();
    if (check(TOKEN_IF)) return parse_if();
    if (check(TOKEN_LOOP)) return parse_loop();
    if (check(TOKEN_FT)) return parse_func_decl();
    if (check(TOKEN_RETURN)) return parse_return();
    if (check(TOKEN_OUTPUT)) return parse_output();
    if (check(TOKEN_TRY)) return parse_try_catch();

    if (check(TOKEN_IDENT) && strcmp(current_tok.lexeme, "global") == 0) {
        int line = current_tok.line;
        parser_advance();
        Stmt *decl = parse_var_decl(1);
        (void)line;
        return decl;
    }

    int line = current_tok.line;
    Expr *expr = parse_expression();
    return make_expr_stmt(expr, line);
}

// --- Entry point ---

StmtList parse_program(void) {
    current_tok = lexer_next_token();

    StmtList program;
    stmt_list_init(&program);

    while (!check(TOKEN_EOF)) {
        if (match(TOKEN_NEWLINE)) continue;
        stmt_list_add(&program, parse_statement());
    }

    lexer_cleanup();
    return program;
}