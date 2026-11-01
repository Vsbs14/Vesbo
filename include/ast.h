#ifndef VESBO_AST_H
#define VESBO_AST_H

#include "token.h"

typedef enum {
    EXPR_NUMBER,
    EXPR_STRING,
    EXPR_BOOL,
    EXPR_NONE,
    EXPR_IDENT,
    EXPR_ARRAY,
    EXPR_MAP,
    EXPR_INDEX,       // arr[i]
    EXPR_BINARY,      // a + b, a equals b, a AND b, etc.
    EXPR_UNARY,       // NOT x, -x
    EXPR_CALL,        // fn(args)
} ExprType;

typedef enum {
    STMT_VAR_DECL,
    STMT_ASSIGN,
    STMT_INDEX_SET,    // set arr[i] to val / set map[k] to val
    STMT_IMPORT,
    STMT_IF,
    STMT_LOOP_TILL,
    STMT_LOOP_WHILE,
    STMT_LOOP_RANGE,   // loop from X to Y as i
    STMT_LOOP_THROUGH, // loop through collection as item
    STMT_FUNC_DECL,
    STMT_RETURN,
    STMT_OUTPUT,
    STMT_TRY_CATCH,
    STMT_EXPR,         // expression used as a statement (e.g. a bare call)
} StmtType;

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQUALS, BIN_NOT_EQUALS,
    BIN_LESS, BIN_LESS_EQUAL,
    BIN_GREATER, BIN_GREATER_EQUAL,
    BIN_AND, BIN_OR,
} BinOp;

typedef enum {
    UN_NOT, UN_NEGATE,
} UnOp;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    Stmt **items;
    int count;
    int capacity;
} StmtList;

typedef struct {
    Expr **items;
    int count;
    int capacity;
} ExprList;

typedef struct {
    Expr *key;
    Expr *value;
} MapEntryExpr;

typedef struct {
    MapEntryExpr *items;
    int count;
    int capacity;
} MapEntryList;

typedef struct {
    char **items;
    int count;
    int capacity;
} StringList;

struct Expr {
    ExprType type;
    int line;
    union {
        double number_val;
        char *string_val;
        int bool_val;

        char *ident_name;

        struct { ExprList elements; } array;
        struct { MapEntryList entries; } map;

        struct { Expr *collection; Expr *index; } index_expr;

        struct { BinOp op; Expr *left; Expr *right; } binary;

        struct { UnOp op; Expr *operand; } unary;

        struct { char *callee_name; ExprList args; } call;
    } as;
};

struct Stmt {
    StmtType type;
    int line;
    union {
        struct { char *name; Expr *value; int is_global; } var_decl;

        struct { char *name; Expr *value; } assign;

        struct { char *name; Expr *index; Expr *value; } index_set;
        struct { char *path; } import_stmt;

        struct {
            Expr *condition;
            StmtList then_branch;
            StmtList else_branch; // empty if no else; may itself contain a single STMT_IF for "else if"
            int has_else;
        } if_stmt;

        struct { Expr *condition; StmtList body; } loop_till;
        struct { Expr *condition; StmtList body; } loop_while;

        struct {
            Expr *from;
            Expr *to;
            char *var_name;
            StmtList body;
        } loop_range;

        struct {
            Expr *collection;
            char *item_name;
            StmtList body;
        } loop_through;

        struct {
            char *name;
            StringList params;
            StmtList body;
        } func_decl;

        struct { Expr *value; } return_stmt; // value may be NULL for bare "return"

        struct { Expr *value; } output_stmt;

        struct {
            StmtList try_body;
            char *error_name;
            StmtList catch_body;
        } try_catch;

        struct { Expr *expr; } expr_stmt;
    } as;
};

Expr *make_number_expr(double val, int line);
Expr *make_string_expr(const char *val, int line);
Expr *make_bool_expr(int val, int line);
Expr *make_none_expr(int line);
Expr *make_ident_expr(const char *name, int line);
Expr *make_array_expr(ExprList elements, int line);
Expr *make_map_expr(MapEntryList entries, int line);
Expr *make_index_expr(Expr *collection, Expr *index, int line);
Expr *make_binary_expr(BinOp op, Expr *left, Expr *right, int line);
Expr *make_unary_expr(UnOp op, Expr *operand, int line);
Expr *make_call_expr(const char *callee_name, ExprList args, int line);

Stmt *make_var_decl_stmt(const char *name, Expr *value, int is_global, int line);
Stmt *make_assign_stmt(const char *name, Expr *value, int line);
Stmt *make_index_set_stmt(const char *name, Expr *index, Expr *value, int line);
Stmt *make_import_stmt(char *path, int line);
Stmt *make_if_stmt(Expr *condition, StmtList then_branch, StmtList else_branch, int has_else, int line);
Stmt *make_loop_till_stmt(Expr *condition, StmtList body, int line);
Stmt *make_loop_while_stmt(Expr *condition, StmtList body, int line);
Stmt *make_loop_range_stmt(Expr *from, Expr *to, const char *var_name, StmtList body, int line);
Stmt *make_loop_through_stmt(Expr *collection, const char *item_name, StmtList body, int line);
Stmt *make_func_decl_stmt(const char *name, StringList params, StmtList body, int line);
Stmt *make_return_stmt(Expr *value, int line);
Stmt *make_output_stmt(Expr *value, int line);
Stmt *make_try_catch_stmt(StmtList try_body, const char *error_name, StmtList catch_body, int line);
Stmt *make_expr_stmt(Expr *expr, int line);

void stmt_list_init(StmtList *list);
void stmt_list_add(StmtList *list, Stmt *stmt);

void expr_list_init(ExprList *list);
void expr_list_add(ExprList *list, Expr *expr);

void map_entry_list_init(MapEntryList *list);
void map_entry_list_add(MapEntryList *list, Expr *key, Expr *value);

void string_list_init(StringList *list);
void string_list_add(StringList *list, const char *str);

void free_expr(Expr *expr);
void free_stmt(Stmt *stmt);
void free_stmt_list(StmtList *list);

#endif