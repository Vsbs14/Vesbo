#include "../include/ast.h"
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

// --- List helpers ---

void stmt_list_init(StmtList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void stmt_list_add(StmtList *list, Stmt *stmt) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Stmt *) * list->capacity);
    }
    list->items[list->count++] = stmt;
}

void expr_list_init(ExprList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void expr_list_add(ExprList *list, Expr *expr) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Expr *) * list->capacity);
    }
    list->items[list->count++] = expr;
}

void string_list_init(StringList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void string_list_add(StringList *list, const char *str) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(char *) * list->capacity);
    }
    list->items[list->count++] = copy_string(str);
}

// --- Expr constructors ---

static Expr *alloc_expr(ExprType type, int line) {
    Expr *e = malloc(sizeof(Expr));
    e->type = type;
    e->line = line;
    return e;
}

Expr *make_number_expr(double val, int line) {
    Expr *e = alloc_expr(EXPR_NUMBER, line);
    e->as.number_val = val;
    return e;
}

Expr *make_string_expr(const char *val, int line) {
    Expr *e = alloc_expr(EXPR_STRING, line);
    e->as.string_val = copy_string(val);
    return e;
}

Expr *make_bool_expr(int val, int line) {
    Expr *e = alloc_expr(EXPR_BOOL, line);
    e->as.bool_val = val;
    return e;
}

Expr *make_none_expr(int line) {
    return alloc_expr(EXPR_NONE, line);
}

Expr *make_ident_expr(const char *name, int line) {
    Expr *e = alloc_expr(EXPR_IDENT, line);
    e->as.ident_name = copy_string(name);
    return e;
}

Expr *make_array_expr(ExprList elements, int line) {
    Expr *e = alloc_expr(EXPR_ARRAY, line);
    e->as.array.elements = elements;
    return e;
}

Expr *make_index_expr(Expr *collection, Expr *index, int line) {
    Expr *e = alloc_expr(EXPR_INDEX, line);
    e->as.index_expr.collection = collection;
    e->as.index_expr.index = index;
    return e;
}

Expr *make_binary_expr(BinOp op, Expr *left, Expr *right, int line) {
    Expr *e = alloc_expr(EXPR_BINARY, line);
    e->as.binary.op = op;
    e->as.binary.left = left;
    e->as.binary.right = right;
    return e;
}

Expr *make_unary_expr(UnOp op, Expr *operand, int line) {
    Expr *e = alloc_expr(EXPR_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.operand = operand;
    return e;
}

Expr *make_call_expr(const char *callee_name, ExprList args, int line) {
    Expr *e = alloc_expr(EXPR_CALL, line);
    e->as.call.callee_name = copy_string(callee_name);
    e->as.call.args = args;
    return e;
}

// --- Stmt constructors ---

static Stmt *alloc_stmt(StmtType type, int line) {
    Stmt *s = malloc(sizeof(Stmt));
    s->type = type;
    s->line = line;
    return s;
}

Stmt *make_var_decl_stmt(const char *name, Expr *value, int is_global, int line) {
    Stmt *s = alloc_stmt(STMT_VAR_DECL, line);
    s->as.var_decl.name = copy_string(name);
    s->as.var_decl.value = value;
    s->as.var_decl.is_global = is_global;
    return s;
}

Stmt *make_assign_stmt(const char *name, Expr *value, int line) {
    Stmt *s = alloc_stmt(STMT_ASSIGN, line);
    s->as.assign.name = copy_string(name);
    s->as.assign.value = value;
    return s;
}

Stmt *make_if_stmt(Expr *condition, StmtList then_branch, StmtList else_branch, int has_else, int line) {
    Stmt *s = alloc_stmt(STMT_IF, line);
    s->as.if_stmt.condition = condition;
    s->as.if_stmt.then_branch = then_branch;
    s->as.if_stmt.else_branch = else_branch;
    s->as.if_stmt.has_else = has_else;
    return s;
}

Stmt *make_loop_till_stmt(Expr *condition, StmtList body, int line) {
    Stmt *s = alloc_stmt(STMT_LOOP_TILL, line);
    s->as.loop_till.condition = condition;
    s->as.loop_till.body = body;
    return s;
}

Stmt *make_loop_while_stmt(Expr *condition, StmtList body, int line) {
    Stmt *s = alloc_stmt(STMT_LOOP_WHILE, line);
    s->as.loop_while.condition = condition;
    s->as.loop_while.body = body;
    return s;
}

Stmt *make_loop_range_stmt(Expr *from, Expr *to, const char *var_name, StmtList body, int line) {
    Stmt *s = alloc_stmt(STMT_LOOP_RANGE, line);
    s->as.loop_range.from = from;
    s->as.loop_range.to = to;
    s->as.loop_range.var_name = copy_string(var_name);
    s->as.loop_range.body = body;
    return s;
}

Stmt *make_loop_through_stmt(Expr *collection, const char *item_name, StmtList body, int line) {
    Stmt *s = alloc_stmt(STMT_LOOP_THROUGH, line);
    s->as.loop_through.collection = collection;
    s->as.loop_through.item_name = copy_string(item_name);
    s->as.loop_through.body = body;
    return s;
}

Stmt *make_func_decl_stmt(const char *name, StringList params, StmtList body, int line) {
    Stmt *s = alloc_stmt(STMT_FUNC_DECL, line);
    s->as.func_decl.name = copy_string(name);
    s->as.func_decl.params = params;
    s->as.func_decl.body = body;
    return s;
}

Stmt *make_return_stmt(Expr *value, int line) {
    Stmt *s = alloc_stmt(STMT_RETURN, line);
    s->as.return_stmt.value = value;
    return s;
}

Stmt *make_output_stmt(Expr *value, int line) {
    Stmt *s = alloc_stmt(STMT_OUTPUT, line);
    s->as.output_stmt.value = value;
    return s;
}

Stmt *make_try_catch_stmt(StmtList try_body, const char *error_name, StmtList catch_body, int line) {
    Stmt *s = alloc_stmt(STMT_TRY_CATCH, line);
    s->as.try_catch.try_body = try_body;
    s->as.try_catch.error_name = copy_string(error_name);
    s->as.try_catch.catch_body = catch_body;
    return s;
}

Stmt *make_expr_stmt(Expr *expr, int line) {
    Stmt *s = alloc_stmt(STMT_EXPR, line);
    s->as.expr_stmt.expr = expr;
    return s;
}

void free_expr(Expr *expr) {
    if (!expr) return;
    switch (expr->type) {
        case EXPR_STRING: free(expr->as.string_val); break;
        case EXPR_IDENT: free(expr->as.ident_name); break;
        case EXPR_ARRAY:
            for (int i = 0; i < expr->as.array.elements.count; i++)
                free_expr(expr->as.array.elements.items[i]);
            free(expr->as.array.elements.items);
            break;
        case EXPR_INDEX:
            free_expr(expr->as.index_expr.collection);
            free_expr(expr->as.index_expr.index);
            break;
        case EXPR_BINARY:
            free_expr(expr->as.binary.left);
            free_expr(expr->as.binary.right);
            break;
        case EXPR_UNARY: free_expr(expr->as.unary.operand); break;
        case EXPR_CALL:
            free(expr->as.call.callee_name);
            for (int i = 0; i < expr->as.call.args.count; i++)
                free_expr(expr->as.call.args.items[i]);
            free(expr->as.call.args.items);
            break;
        case EXPR_NUMBER:
        case EXPR_BOOL:
        case EXPR_NONE:
            break;
    }
    free(expr);
}

void free_stmt_list(StmtList *list) {
    for (int i = 0; i < list->count; i++) free_stmt(list->items[i]);
    free(list->items);
}

static void free_string_list(StringList *list) {
    for (int i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
}

void free_stmt(Stmt *stmt) {
    if (!stmt) return;
    switch (stmt->type) {
        case STMT_VAR_DECL:
            free(stmt->as.var_decl.name);
            free_expr(stmt->as.var_decl.value);
            break;
        case STMT_ASSIGN:
            free(stmt->as.assign.name);
            free_expr(stmt->as.assign.value);
            break;
        case STMT_IF:
            free_expr(stmt->as.if_stmt.condition);
            free_stmt_list(&stmt->as.if_stmt.then_branch);
            free_stmt_list(&stmt->as.if_stmt.else_branch);
            break;
        case STMT_LOOP_TILL:
            free_expr(stmt->as.loop_till.condition);
            free_stmt_list(&stmt->as.loop_till.body);
            break;
        case STMT_LOOP_WHILE:
            free_expr(stmt->as.loop_while.condition);
            free_stmt_list(&stmt->as.loop_while.body);
            break;
        case STMT_LOOP_RANGE:
            free_expr(stmt->as.loop_range.from);
            free_expr(stmt->as.loop_range.to);
            free(stmt->as.loop_range.var_name);
            free_stmt_list(&stmt->as.loop_range.body);
            break;
        case STMT_LOOP_THROUGH:
            free_expr(stmt->as.loop_through.collection);
            free(stmt->as.loop_through.item_name);
            free_stmt_list(&stmt->as.loop_through.body);
            break;
        case STMT_FUNC_DECL:
            free(stmt->as.func_decl.name);
            free_string_list(&stmt->as.func_decl.params);
            free_stmt_list(&stmt->as.func_decl.body);
            break;
        case STMT_RETURN: free_expr(stmt->as.return_stmt.value); break;
        case STMT_OUTPUT: free_expr(stmt->as.output_stmt.value); break;
        case STMT_TRY_CATCH:
            free_stmt_list(&stmt->as.try_catch.try_body);
            free(stmt->as.try_catch.error_name);
            free_stmt_list(&stmt->as.try_catch.catch_body);
            break;
        case STMT_EXPR: free_expr(stmt->as.expr_stmt.expr); break;
    }
    free(stmt);
}