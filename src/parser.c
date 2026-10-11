#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/token.h"
#include "../include/module.h"
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
    ast_set_current_loc(module_get_current_file(), previous_tok.line, previous_tok.column);
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
    const char *file = module_get_current_file();
    if (file && file[0]) {
        fprintf(stderr, "Parse error at %s:%d:%d (near '%s'): %s\n",
                file, tok->line, tok->column, tok->lexeme ? tok->lexeme : "?", message);
    } else {
        fprintf(stderr, "Parse error at line %d:%d (near '%s'): %s\n",
                tok->line, tok->column, tok->lexeme ? tok->lexeme : "?", message);
    }
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
    for (;;) {
        if (match(TOKEN_LBRACKET)) {
            int idx_line = previous_tok.line;
            int idx_col = previous_tok.column;
            Expr *index = parse_expression();
            consume(TOKEN_RBRACKET, "expected ']' after index");
            ast_set_current_loc(module_get_current_file(), idx_line, idx_col);
            expr = make_index_expr(expr, index, idx_line);
        } else if (match(TOKEN_DOT)) {
            int dot_line = previous_tok.line;
            int dot_col = previous_tok.column;
            Token member = consume(TOKEN_IDENT, "expected field or method name after '.'");
            if (match(TOKEN_LPAREN)) {
                ExprList args;
                expr_list_init(&args);
                if (!check(TOKEN_RPAREN)) {
                    do {
                        expr_list_add(&args, parse_expression());
                    } while (match(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "expected ')' after method arguments");
                ast_set_current_loc(module_get_current_file(), dot_line, dot_col);
                expr = make_method_call_expr(expr, member.lexeme, args, dot_line);
            } else {
                ast_set_current_loc(module_get_current_file(), dot_line, dot_col);
                expr = make_get_field_expr(expr, member.lexeme, dot_line);
            }
        } else {
            break;
        }
    }
    (void)line;
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
    int col = current_tok.column;
    ast_set_current_loc(module_get_current_file(), line, col);

    if (match(TOKEN_NUMBER)) {
        return make_number_expr(previous_tok.number_val, line);
    }
    if (match(TOKEN_STRING)) {
        return parse_postfix(make_string_expr(previous_tok.lexeme, line), line);
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
    if (match(TOKEN_YIELD)) {
        if (check(TOKEN_NEWLINE) || check(TOKEN_DEDENT) || check(TOKEN_EOF) ||
            check(TOKEN_RPAREN) || check(TOKEN_RBRACKET) || check(TOKEN_RBRACE) || check(TOKEN_COMMA)) {
            return make_yield_expr(NULL, line);
        }
        if (match(TOKEN_LPAREN)) {
            if (match(TOKEN_RPAREN)) {
                return make_yield_expr(NULL, line);
            }
            Expr *val = parse_expression();
            consume(TOKEN_RPAREN, "expected ')' after yield argument");
            return make_yield_expr(val, line);
        }
        Expr *val = parse_expression();
        return make_yield_expr(val, line);
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
        ast_set_current_loc(module_get_current_file(), line, col);
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
        ast_set_current_loc(module_get_current_file(), line, col);
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
            ast_set_current_loc(module_get_current_file(), line, col);
            expr = make_call_expr(name, args, line);
        } else {
            ast_set_current_loc(module_get_current_file(), line, col);
            expr = make_ident_expr(name, line);
        }

        return parse_postfix(expr, line);
    }

    error_at(&current_tok, "expected an expression");
    return NULL;
}

static Expr *parse_unary(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    if (match(TOKEN_NOT)) {
        Expr *operand = parse_unary();
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_unary_expr(UN_NOT, operand, line);
    }
    if (match(TOKEN_MINUS)) {
        Expr *operand = parse_unary();
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_unary_expr(UN_NEGATE, operand, line);
    }
    return parse_primary();
}

static Expr *parse_multiplicative(void) {
    Expr *left = parse_unary();
    for (;;) {
        int line = current_tok.line;
        int col = current_tok.column;
        if (match(TOKEN_STAR)) {
            Expr *right = parse_unary();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_MUL, left, right, line);
        } else if (match(TOKEN_SLASH)) {
            Expr *right = parse_unary();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_DIV, left, right, line);
        } else if (match(TOKEN_PERCENT)) {
            Expr *right = parse_unary();
            ast_set_current_loc(module_get_current_file(), line, col);
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
        int col = current_tok.column;
        if (match(TOKEN_PLUS)) {
            Expr *right = parse_multiplicative();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_ADD, left, right, line);
        } else if (match(TOKEN_MINUS)) {
            Expr *right = parse_multiplicative();
            ast_set_current_loc(module_get_current_file(), line, col);
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
        int col = current_tok.column;
        if (match(TOKEN_LESS_THAN)) {
            Expr *r = parse_additive();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_LESS, left, r, line);
        } else if (match(TOKEN_GREATER_THAN)) {
            Expr *r = parse_additive();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_GREATER, left, r, line);
        } else if (match(TOKEN_LESS_THAN_OR_EQUAL)) {
            Expr *r = parse_additive();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_LESS_EQUAL, left, r, line);
        } else if (match(TOKEN_GREATER_THAN_OR_EQUAL)) {
            Expr *r = parse_additive();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_GREATER_EQUAL, left, r, line);
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
        int col = current_tok.column;
        if (match(TOKEN_EQUALS)) {
            Expr *r = parse_comparison();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_EQUALS, left, r, line);
        } else if (match(TOKEN_DOES_NOT_EQUAL)) {
            Expr *r = parse_comparison();
            ast_set_current_loc(module_get_current_file(), line, col);
            left = make_binary_expr(BIN_NOT_EQUALS, left, r, line);
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
        int col = previous_tok.column;
        Expr *right = parse_equality();
        ast_set_current_loc(module_get_current_file(), line, col);
        left = make_binary_expr(BIN_AND, left, right, line);
    }
    return left;
}

static Expr *parse_or(void) {
    Expr *left = parse_and();
    while (match(TOKEN_OR)) {
        int line = previous_tok.line;
        int col = previous_tok.column;
        Expr *right = parse_and();
        ast_set_current_loc(module_get_current_file(), line, col);
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
    int col = current_tok.column;
    consume(TOKEN_VAR, "expected 'var'");
    Token name_tok = consume(TOKEN_IDENT, "expected variable name");
    if (!match(TOKEN_IS) && !match(TOKEN_ASSIGN)) {
        error_at(&current_tok, "expected 'is' or '=' after variable name");
    }
    Expr *value = parse_expression();
    ast_set_current_loc(module_get_current_file(), line, col);
    return make_var_decl_stmt(name_tok.lexeme, value, is_global, line);
}

static Stmt *target_expr_to_assign_stmt(Expr *target, Expr *value, int line, int col) {
    ast_set_current_loc(module_get_current_file(), line, col);
    if (target->type == EXPR_IDENT) {
        Stmt *s = make_assign_stmt(target->as.ident_name, value, line);
        free_expr(target);
        return s;
    } else if (target->type == EXPR_GET_FIELD) {
        Expr *obj = target->as.get_field.object;
        Stmt *s = make_set_field_stmt(obj, target->as.get_field.field_name, value, line);
        target->as.get_field.object = NULL;
        free_expr(target);
        return s;
    } else if (target->type == EXPR_INDEX) {
        Expr *coll = target->as.index_expr.collection;
        Expr *idx = target->as.index_expr.index;
        target->as.index_expr.collection = NULL;
        target->as.index_expr.index = NULL;
        free_expr(target);
        return make_index_set_target_stmt(coll, idx, value, line);
    }
    error_at(&current_tok, "invalid assignment target");
    return NULL;
}

static Stmt *parse_set(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    consume(TOKEN_SET, "expected 'set'");
    Expr *target = parse_primary();
    if (!match(TOKEN_TO) && !match(TOKEN_ASSIGN)) {
        error_at(&current_tok, "expected 'to' after target in 'set' statement");
    }
    Expr *value = parse_expression();
    return target_expr_to_assign_stmt(target, value, line, col);
}

static Stmt *parse_if(void) {
    int line = current_tok.line;
    int col = current_tok.column;
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

    ast_set_current_loc(module_get_current_file(), line, col);
    return make_if_stmt(condition, then_branch, else_branch, has_else, line);
}

static Stmt *parse_loop(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    consume(TOKEN_LOOP, "expected 'loop'");

    if (match(TOKEN_TILL)) {
        Expr *condition = parse_expression();
        StmtList body = parse_block();
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_loop_till_stmt(condition, body, line);
    }

    if (match(TOKEN_WHILE)) {
        Expr *condition = parse_expression();
        StmtList body = parse_block();
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_loop_while_stmt(condition, body, line);
    }

    if (match(TOKEN_FROM)) {
        Expr *from = parse_expression();
        consume(TOKEN_TO, "expected 'to' in 'loop from X to Y'");
        Expr *to = parse_expression();
        consume(TOKEN_AS, "expected 'as' in 'loop from X to Y as name'");
        Token var_tok = consume(TOKEN_IDENT, "expected loop variable name after 'as'");
        StmtList body = parse_block();
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_loop_range_stmt(from, to, var_tok.lexeme, body, line);
    }

    if (match(TOKEN_THROUGH)) {
        Expr *collection = parse_expression();
        consume(TOKEN_AS, "expected 'as' in 'loop through X as name'");
        Token item_tok = consume(TOKEN_IDENT, "expected item name after 'as'");
        StmtList body = parse_block();
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_loop_through_stmt(collection, item_tok.lexeme, body, line);
    }

    error_at(&current_tok, "expected 'while', 'till', 'from', or 'through' after 'loop'");
    return NULL;
}

static Stmt *parse_func_decl(void) {
    int line = current_tok.line;
    int col = current_tok.column;
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
    ast_set_current_loc(module_get_current_file(), line, col);
    return make_func_decl_stmt(name_tok.lexeme, params, body, line);
}

static Stmt *parse_struct_decl(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    consume(TOKEN_STRUCT, "expected 'struct'");
    Token name_tok = consume(TOKEN_IDENT, "expected struct name");

    StringList field_names;
    string_list_init(&field_names);
    ExprList field_defaults;
    expr_list_init(&field_defaults);
    StmtList methods;
    stmt_list_init(&methods);

    /* Optional inline field list: struct Vec2(x, y) */
    if (match(TOKEN_LPAREN)) {
        if (!check(TOKEN_RPAREN)) {
            do {
                Token f = consume(TOKEN_IDENT, "expected field name");
                string_list_add(&field_names, f.lexeme);
                if (match(TOKEN_IS) || match(TOKEN_ASSIGN)) {
                    expr_list_add(&field_defaults, parse_expression());
                } else {
                    expr_list_add(&field_defaults, make_none_expr(f.line));
                }
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RPAREN, "expected ')' after struct fields");
    }

    /* Check if an indented body follows */
    if (check(TOKEN_NEWLINE)) {
        consume(TOKEN_NEWLINE, "expected newline after struct header");
        if (match(TOKEN_INDENT)) {
            while (!check(TOKEN_DEDENT) && !check(TOKEN_EOF)) {
                if (match(TOKEN_NEWLINE)) continue;

                if (check(TOKEN_FT)) {
                    Stmt *method = parse_func_decl();
                    if (method->as.func_decl.params.count == 0 ||
                        strcmp(method->as.func_decl.params.items[0], "self") != 0) {
                        string_list_insert_first(&method->as.func_decl.params, "self");
                    }
                    stmt_list_add(&methods, method);
                } else if (match(TOKEN_VAR)) {
                    Token f = consume(TOKEN_IDENT, "expected field name");
                    string_list_add(&field_names, f.lexeme);
                    if (match(TOKEN_IS) || match(TOKEN_ASSIGN)) {
                        expr_list_add(&field_defaults, parse_expression());
                    } else {
                        expr_list_add(&field_defaults, make_none_expr(f.line));
                    }
                } else if (check(TOKEN_IDENT)) {
                    do {
                        Token f = consume(TOKEN_IDENT, "expected field name");
                        string_list_add(&field_names, f.lexeme);
                        if (match(TOKEN_IS) || match(TOKEN_ASSIGN) || match(TOKEN_COLON)) {
                            expr_list_add(&field_defaults, parse_expression());
                        } else {
                            expr_list_add(&field_defaults, make_none_expr(f.line));
                        }
                    } while (match(TOKEN_COMMA));
                } else {
                    error_at(&current_tok, "expected field or method ('fnc') inside struct");
                }
            }
            consume(TOKEN_DEDENT, "expected struct indentation to end");
        }
    }

    ast_set_current_loc(module_get_current_file(), line, col);
    return make_struct_decl_stmt(name_tok.lexeme, field_names, field_defaults, methods, line);
}

static Stmt *parse_return(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    consume(TOKEN_RETURN, "expected 'return'");
    if (check(TOKEN_NEWLINE) || check(TOKEN_DEDENT) || check(TOKEN_EOF)) {
        ast_set_current_loc(module_get_current_file(), line, col);
        return make_return_stmt(NULL, line);
    }
    Expr *value = parse_expression();
    ast_set_current_loc(module_get_current_file(), line, col);
    return make_return_stmt(value, line);
}

static Stmt *parse_output(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    consume(TOKEN_OUTPUT, "expected 'output'");
    Expr *value = parse_expression();
    ast_set_current_loc(module_get_current_file(), line, col);
    return make_output_stmt(value, line);
}

static Stmt *parse_try_catch(void) {
    int line = current_tok.line;
    int col = current_tok.column;
    consume(TOKEN_TRY, "expected 'try'");
    StmtList try_body = parse_block();
    consume(TOKEN_CATCH, "expected 'catch' after try block");
    Token error_tok = consume(TOKEN_IDENT, "expected error variable name after 'catch'");
    StmtList catch_body = parse_block();
    ast_set_current_loc(module_get_current_file(), line, col);
    return make_try_catch_stmt(try_body, error_tok.lexeme, catch_body, line);
}

static Stmt *parse_import(void) {
    int line = current_tok.line;
    int col = current_tok.column;
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
    ast_set_current_loc(module_get_current_file(), line, col);
    return make_import_stmt(path, line);
}

static Stmt *parse_statement(void) {
    if (check(TOKEN_IMPORT)) return parse_import();
    if (check(TOKEN_VAR)) return parse_var_decl(0);
    if (check(TOKEN_SET)) return parse_set();
    if (check(TOKEN_IF)) return parse_if();
    if (check(TOKEN_LOOP)) return parse_loop();
    if (check(TOKEN_FT)) return parse_func_decl();
    if (check(TOKEN_STRUCT)) return parse_struct_decl();
    if (check(TOKEN_RETURN)) return parse_return();
    if (check(TOKEN_OUTPUT)) return parse_output();
    if (check(TOKEN_TRY)) return parse_try_catch();

    if (check(TOKEN_IDENT) && strcmp(current_tok.lexeme, "global") == 0) {
        parser_advance();
        return parse_var_decl(1);
    }

    int line = current_tok.line;
    int col = current_tok.column;
    Expr *expr = parse_expression();

    /* Support direct '=' assignment: e.g. camera.position.x = 10 or x = 5 or arr[0] = 1 */
    if (match(TOKEN_ASSIGN)) {
        Expr *val = parse_expression();
        return target_expr_to_assign_stmt(expr, val, line, col);
    }

    ast_set_current_loc(module_get_current_file(), line, col);
    return make_expr_stmt(expr, line);
}

// --- Entry point ---

StmtList parse_program(void) {
    current_tok = lexer_next_token();
    ast_set_current_loc(module_get_current_file(), current_tok.line, current_tok.column);

    StmtList program;
    stmt_list_init(&program);

    while (!check(TOKEN_EOF)) {
        if (match(TOKEN_NEWLINE)) continue;
        stmt_list_add(&program, parse_statement());
    }

    lexer_cleanup();
    return program;
}