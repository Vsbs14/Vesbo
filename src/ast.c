#include "../include/ast.h"
#include "../include/module.h"
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

static char current_ast_file[4096] = "<script>";
static int current_ast_line = 1;
static int current_ast_col = 1;

void ast_set_current_loc(const char *file, int line, int column) {
    const char *f = file ? file : module_get_current_file();
    if (f && f[0]) {
        strncpy(current_ast_file, f, sizeof(current_ast_file) - 1);
        current_ast_file[sizeof(current_ast_file) - 1] = '\0';
    } else {
        strcpy(current_ast_file, "<script>");
    }
    if (line > 0) current_ast_line = line;
    if (column > 0) current_ast_col = column;
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

void string_list_insert_first(StringList *list, const char *str) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(char *) * list->capacity);
    }
    for (int i = list->count; i > 0; i--) {
        list->items[i] = list->items[i - 1];
    }
    list->items[0] = copy_string(str);
    list->count++;
}

// --- Expr constructors ---

static Expr *alloc_expr(ExprType type, int line) {
    Expr *e = malloc(sizeof(Expr));
    e->type = type;
    e->line = line > 0 ? line : current_ast_line;
    e->column = current_ast_col;
    const char *f = module_get_current_file();
    e->file = copy_string((f && f[0]) ? f : current_ast_file);
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

Expr *make_map_expr(MapEntryList entries, int line) {
    Expr *e = alloc_expr(EXPR_MAP, line);
    e->as.map.entries = entries;
    return e;
}

void map_entry_list_init(MapEntryList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void map_entry_list_add(MapEntryList *list, Expr *key, Expr *value) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, list->capacity * sizeof(MapEntryExpr));
    }
    list->items[list->count].key = key;
    list->items[list->count].value = value;
    list->count++;
}

Expr *make_index_expr(Expr *collection, Expr *index, int line) {
    Expr *e = alloc_expr(EXPR_INDEX, line);
    e->as.index_expr.collection = collection;
    e->as.index_expr.index = index;
    return e;
}

Expr *make_get_field_expr(Expr *object, const char *field_name, int line) {
    Expr *e = alloc_expr(EXPR_GET_FIELD, line);
    e->as.get_field.object = object;
    e->as.get_field.field_name = copy_string(field_name);
    return e;
}

Expr *make_method_call_expr(Expr *object, const char *method_name, ExprList args, int line) {
    Expr *e = alloc_expr(EXPR_METHOD_CALL, line);
    e->as.method_call.object = object;
    e->as.method_call.method_name = copy_string(method_name);
    e->as.method_call.args = args;
    return e;
}

Expr *make_yield_expr(Expr *value, int line) {
    Expr *e = alloc_expr(EXPR_YIELD, line);
    e->as.yield_expr.value = value;
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
    s->line = line > 0 ? line : current_ast_line;
    s->column = current_ast_col;
    const char *f = module_get_current_file();
    s->file = copy_string((f && f[0]) ? f : current_ast_file);
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

Stmt *make_index_set_stmt(const char *name, Expr *index, Expr *value, int line) {
    Stmt *s = alloc_stmt(STMT_INDEX_SET, line);
    s->as.index_set.name = copy_string(name);
    s->as.index_set.collection = NULL;
    s->as.index_set.index = index;
    s->as.index_set.value = value;
    return s;
}

Stmt *make_index_set_target_stmt(Expr *collection, Expr *index, Expr *value, int line) {
    Stmt *s = alloc_stmt(STMT_INDEX_SET, line);
    if (collection && collection->type == EXPR_IDENT) {
        s->as.index_set.name = copy_string(collection->as.ident_name);
        s->as.index_set.collection = NULL;
        free_expr(collection);
    } else {
        s->as.index_set.name = NULL;
        s->as.index_set.collection = collection;
    }
    s->as.index_set.index = index;
    s->as.index_set.value = value;
    return s;
}

Stmt *make_set_field_stmt(Expr *object, const char *field_name, Expr *value, int line) {
    Stmt *s = alloc_stmt(STMT_SET_FIELD, line);
    s->as.set_field.object = object;
    s->as.set_field.field_name = copy_string(field_name);
    s->as.set_field.value = value;
    return s;
}

Stmt *make_struct_decl_stmt(const char *name, StringList field_names, ExprList field_defaults, StmtList methods, int line) {
    Stmt *s = alloc_stmt(STMT_STRUCT_DECL, line);
    s->as.struct_decl.name = copy_string(name);
    s->as.struct_decl.field_names = field_names;
    s->as.struct_decl.field_defaults = field_defaults;
    s->as.struct_decl.methods = methods;
    return s;
}

Stmt *make_import_stmt(char *path, int line) {
    Stmt *s = alloc_stmt(STMT_IMPORT, line);
    s->as.import_stmt.path = path;
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

Stmt *make_break_stmt(int line) {
    return alloc_stmt(STMT_BREAK, line);
}

Stmt *make_continue_stmt(int line) {
    return alloc_stmt(STMT_CONTINUE, line);
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

static void free_string_list(StringList *list) {
    for (int i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
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
        case EXPR_MAP:
            for (int i = 0; i < expr->as.map.entries.count; i++) {
                free_expr(expr->as.map.entries.items[i].key);
                free_expr(expr->as.map.entries.items[i].value);
            }
            free(expr->as.map.entries.items);
            break;
        case EXPR_INDEX:
            free_expr(expr->as.index_expr.collection);
            free_expr(expr->as.index_expr.index);
            break;
        case EXPR_GET_FIELD:
            free_expr(expr->as.get_field.object);
            free(expr->as.get_field.field_name);
            break;
        case EXPR_METHOD_CALL:
            free_expr(expr->as.method_call.object);
            free(expr->as.method_call.method_name);
            for (int i = 0; i < expr->as.method_call.args.count; i++)
                free_expr(expr->as.method_call.args.items[i]);
            free(expr->as.method_call.args.items);
            break;
        case EXPR_YIELD:
            free_expr(expr->as.yield_expr.value);
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
    free(expr->file);
    free(expr);
}

void free_stmt_list(StmtList *list) {
    for (int i = 0; i < list->count; i++) free_stmt(list->items[i]);
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
        case STMT_INDEX_SET:
            free(stmt->as.index_set.name);
            free_expr(stmt->as.index_set.collection);
            free_expr(stmt->as.index_set.index);
            free_expr(stmt->as.index_set.value);
            break;
        case STMT_SET_FIELD:
            free_expr(stmt->as.set_field.object);
            free(stmt->as.set_field.field_name);
            free_expr(stmt->as.set_field.value);
            break;
        case STMT_STRUCT_DECL:
            free(stmt->as.struct_decl.name);
            free_string_list(&stmt->as.struct_decl.field_names);
            for (int i = 0; i < stmt->as.struct_decl.field_defaults.count; i++)
                free_expr(stmt->as.struct_decl.field_defaults.items[i]);
            free(stmt->as.struct_decl.field_defaults.items);
            free_stmt_list(&stmt->as.struct_decl.methods);
            break;
        case STMT_IMPORT:
            free(stmt->as.import_stmt.path);
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
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case STMT_OUTPUT: free_expr(stmt->as.output_stmt.value); break;
        case STMT_TRY_CATCH:
            free_stmt_list(&stmt->as.try_catch.try_body);
            free(stmt->as.try_catch.error_name);
            free_stmt_list(&stmt->as.try_catch.catch_body);
            break;
        case STMT_EXPR: free_expr(stmt->as.expr_stmt.expr); break;
    }
    free(stmt->file);
    free(stmt);
}

Expr *copy_expr(const Expr *expr) {
    if (!expr) return NULL;
    Expr *copy = alloc_expr(expr->type, expr->line);
    copy->column = expr->column;
    if (expr->file) {
        if (copy->file) free(copy->file);
        copy->file = copy_string(expr->file);
    }

    switch (expr->type) {
        case EXPR_NUMBER:
            copy->as.number_val = expr->as.number_val;
            break;
        case EXPR_STRING:
            copy->as.string_val = copy_string(expr->as.string_val);
            break;
        case EXPR_BOOL:
            copy->as.bool_val = expr->as.bool_val;
            break;
        case EXPR_NONE:
            break;
        case EXPR_IDENT:
            copy->as.ident_name = copy_string(expr->as.ident_name);
            break;
        case EXPR_ARRAY:
            expr_list_init(&copy->as.array.elements);
            for (int i = 0; i < expr->as.array.elements.count; i++) {
                expr_list_add(&copy->as.array.elements, copy_expr(expr->as.array.elements.items[i]));
            }
            break;
        case EXPR_MAP:
            map_entry_list_init(&copy->as.map.entries);
            for (int i = 0; i < expr->as.map.entries.count; i++) {
                map_entry_list_add(&copy->as.map.entries,
                                   copy_expr(expr->as.map.entries.items[i].key),
                                   copy_expr(expr->as.map.entries.items[i].value));
            }
            break;
        case EXPR_INDEX:
            copy->as.index_expr.collection = copy_expr(expr->as.index_expr.collection);
            copy->as.index_expr.index = copy_expr(expr->as.index_expr.index);
            break;
        case EXPR_GET_FIELD:
            copy->as.get_field.object = copy_expr(expr->as.get_field.object);
            copy->as.get_field.field_name = copy_string(expr->as.get_field.field_name);
            break;
        case EXPR_METHOD_CALL:
            copy->as.method_call.object = copy_expr(expr->as.method_call.object);
            copy->as.method_call.method_name = copy_string(expr->as.method_call.method_name);
            expr_list_init(&copy->as.method_call.args);
            for (int i = 0; i < expr->as.method_call.args.count; i++) {
                expr_list_add(&copy->as.method_call.args, copy_expr(expr->as.method_call.args.items[i]));
            }
            break;
        case EXPR_YIELD:
            copy->as.yield_expr.value = copy_expr(expr->as.yield_expr.value);
            break;
        case EXPR_BINARY:
            copy->as.binary.op = expr->as.binary.op;
            copy->as.binary.left = copy_expr(expr->as.binary.left);
            copy->as.binary.right = copy_expr(expr->as.binary.right);
            break;
        case EXPR_UNARY:
            copy->as.unary.op = expr->as.unary.op;
            copy->as.unary.operand = copy_expr(expr->as.unary.operand);
            break;
        case EXPR_CALL:
            copy->as.call.callee_name = copy_string(expr->as.call.callee_name);
            expr_list_init(&copy->as.call.args);
            for (int i = 0; i < expr->as.call.args.count; i++) {
                expr_list_add(&copy->as.call.args, copy_expr(expr->as.call.args.items[i]));
            }
            break;
    }
    return copy;
}