#include "../include/interpreter.h"
#include "../include/environment.h"
#include "../include/value.h"
#include "../include/builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    char *name;
    StringList params;
    StmtList body;
} FuncDef;

static FuncDef *functions = NULL;
static int func_count = 0;
static int func_capacity = 0;

static char *copy_string(const char *source) {
    size_t length = strlen(source);
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, source, length + 1);
    return copy;
}

static void register_function(const char *name, StringList params, StmtList body) {
    if (func_count >= func_capacity) {
        func_capacity = func_capacity == 0 ? 8 : func_capacity * 2;
        functions = realloc(functions, sizeof(FuncDef) * func_capacity);
    }
    functions[func_count].name = copy_string(name);
    functions[func_count].params = params;
    functions[func_count].body = body;
    func_count++;
}

static FuncDef *find_function(const char *name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) return &functions[i];
    }
    return NULL;
}

static void clear_functions(void) {
    for (int i = 0; i < func_count; i++) free(functions[i].name);
    free(functions);
    functions = NULL;
    func_count = 0;
    func_capacity = 0;
}

typedef enum {
    SIGNAL_NONE,
    SIGNAL_RETURN,
    SIGNAL_ERROR,
} Signal;

typedef struct {
    Signal signal;
    Value value;
    char *error_message;
} ExecResult;

static ExecResult exec_none(void) {
    ExecResult r;
    r.signal = SIGNAL_NONE;
    r.value = make_none_value();
    r.error_message = NULL;
    return r;
}

static ExecResult exec_return(Value v) {
    ExecResult r;
    r.signal = SIGNAL_RETURN;
    r.value = v;
    r.error_message = NULL;
    return r;
}

static ExecResult exec_error(const char *message) {
    ExecResult r;
    r.signal = SIGNAL_ERROR;
    r.value = make_string_value(message);
    r.error_message = copy_string(message);
    return r;
}

static Value eval_expr(Expr *expr, Environment *env, ExecResult *result);
static ExecResult exec_stmt(Stmt *stmt, Environment *env);
static ExecResult exec_block(StmtList block, Environment *env);

static Value eval_binary(Expr *expr, Environment *env, ExecResult *result) {
    Value left = eval_expr(expr->as.binary.left, env, result);
    if (result->signal != SIGNAL_NONE) return make_none_value();

    if (expr->as.binary.op == BIN_AND) {
        if (!value_is_truthy(left)) return make_bool_value(0);
        Value right = eval_expr(expr->as.binary.right, env, result);
        if (result->signal != SIGNAL_NONE) return make_none_value();
        return make_bool_value(value_is_truthy(right));
    }
    if (expr->as.binary.op == BIN_OR) {
        if (value_is_truthy(left)) return make_bool_value(1);
        Value right = eval_expr(expr->as.binary.right, env, result);
        if (result->signal != SIGNAL_NONE) return make_none_value();
        return make_bool_value(value_is_truthy(right));
    }

    Value right = eval_expr(expr->as.binary.right, env, result);
    if (result->signal != SIGNAL_NONE) return make_none_value();

    switch (expr->as.binary.op) {
        case BIN_ADD:
            if (left.type == VAL_STRING || right.type == VAL_STRING) {
                char *ls = value_to_display_string(left);
                char *rs = value_to_display_string(right);
                size_t total = strlen(ls) + strlen(rs) + 1;
                char *combined = malloc(total);
                snprintf(combined, total, "%s%s", ls, rs);
                Value v = make_string_value(combined);
                free(ls); free(rs); free(combined);
                return v;
            }
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                *result = exec_error("'+' requires two numbers (or a string)");
                return make_none_value();
            }
            return make_number_value(left.as.number + right.as.number);

        case BIN_SUB:
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                *result = exec_error("'-' requires two numbers");
                return make_none_value();
            }
            return make_number_value(left.as.number - right.as.number);

        case BIN_MUL:
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                *result = exec_error("'*' requires two numbers");
                return make_none_value();
            }
            return make_number_value(left.as.number * right.as.number);

        case BIN_DIV:
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                *result = exec_error("'/' requires two numbers");
                return make_none_value();
            }
            if (right.as.number == 0) {
                *result = exec_error("division by zero");
                return make_none_value();
            }
            return make_number_value(left.as.number / right.as.number);

        case BIN_MOD:
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                *result = exec_error("'%' requires two numbers");
                return make_none_value();
            }
            if (right.as.number == 0) {
                *result = exec_error("division by zero");
                return make_none_value();
            }
            return make_number_value(fmod(left.as.number, right.as.number));

        case BIN_EQUALS:
            return make_bool_value(values_equal(left, right));
        case BIN_NOT_EQUALS:
            return make_bool_value(!values_equal(left, right));

        case BIN_LESS:
        case BIN_LESS_EQUAL:
        case BIN_GREATER:
        case BIN_GREATER_EQUAL:
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                *result = exec_error("comparison operators require two numbers");
                return make_none_value();
            }
            switch (expr->as.binary.op) {
                case BIN_LESS:          return make_bool_value(left.as.number <  right.as.number);
                case BIN_LESS_EQUAL:    return make_bool_value(left.as.number <= right.as.number);
                case BIN_GREATER:       return make_bool_value(left.as.number >  right.as.number);
                case BIN_GREATER_EQUAL: return make_bool_value(left.as.number >= right.as.number);
                default: break;
            }
            break;

        case BIN_AND:
        case BIN_OR:
            break;
    }

    *result = exec_error("unknown binary operator");
    return make_none_value();
}

static Value eval_unary(Expr *expr, Environment *env, ExecResult *result) {
    Value operand = eval_expr(expr->as.unary.operand, env, result);
    if (result->signal != SIGNAL_NONE) return make_none_value();

    if (expr->as.unary.op == UN_NOT) {
        return make_bool_value(!value_is_truthy(operand));
    }
    if (operand.type != VAL_NUMBER) {
        *result = exec_error("unary '-' requires a number");
        return make_none_value();
    }
    return make_number_value(-operand.as.number);
}

static Value call_user_function(FuncDef *fn, Value *args, int arg_count, Environment *env, ExecResult *result) {
    if (arg_count != fn->params.count) {
        char msg[256];
        snprintf(msg, sizeof(msg), "function '%s' expects %d argument(s), got %d",
                 fn->name, fn->params.count, arg_count);
        *result = exec_error(msg);
        return make_none_value();
    }

    env_push_scope(env);
    for (int i = 0; i < arg_count; i++) {
        env_declare(env, fn->params.items[i], args[i], 0);
    }

    ExecResult body_result = exec_block(fn->body, env);
    env_pop_scope(env);

    if (body_result.signal == SIGNAL_ERROR) {
        *result = body_result;
        return make_none_value();
    }
    if (body_result.signal == SIGNAL_RETURN) {
        return body_result.value;
    }
    return make_none_value();
}

static Value eval_call(Expr *expr, Environment *env, ExecResult *result) {
    int arg_count = expr->as.call.args.count;
    Value *args = malloc(sizeof(Value) * (arg_count > 0 ? arg_count : 1));
    for (int i = 0; i < arg_count; i++) {
        args[i] = eval_expr(expr->as.call.args.items[i], env, result);
        if (result->signal != SIGNAL_NONE) { free(args); return make_none_value(); }
    }

    Value out;
    const char *builtin_error = NULL;
    int builtin_status = call_builtin(expr->as.call.callee_name, args, arg_count,
                                      &out, &builtin_error);
    if (builtin_status == 1) {
        free(args);
        return out;
    }
    if (builtin_status == -1) {
        *result = exec_error(builtin_error);
        free(args);
        return make_none_value();
    }

    FuncDef *fn = find_function(expr->as.call.callee_name);
    if (!fn) {
        char msg[256];
        snprintf(msg, sizeof(msg), "undefined function '%s'", expr->as.call.callee_name);
        *result = exec_error(msg);
        free(args);
        return make_none_value();
    }

    Value ret = call_user_function(fn, args, arg_count, env, result);
    free(args);
    return ret;
}

static Value eval_expr(Expr *expr, Environment *env, ExecResult *result) {
    switch (expr->type) {
        case EXPR_NUMBER: return make_number_value(expr->as.number_val);
        case EXPR_STRING: return make_string_value(expr->as.string_val);
        case EXPR_BOOL:   return make_bool_value(expr->as.bool_val);
        case EXPR_NONE:   return make_none_value();

        case EXPR_IDENT: {
            Value v;
            if (!env_get(env, expr->as.ident_name, &v)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "undefined variable '%s'", expr->as.ident_name);
                *result = exec_error(msg);
                return make_none_value();
            }
            return v;
        }

        case EXPR_ARRAY: {
            Value arr = make_array_value();
            for (int i = 0; i < expr->as.array.elements.count; i++) {
                Value item = eval_expr(expr->as.array.elements.items[i], env, result);
                if (result->signal != SIGNAL_NONE) return make_none_value();
                value_array_push(&arr.as.array, item);
            }
            return arr;
        }

        case EXPR_INDEX: {
            Value collection = eval_expr(expr->as.index_expr.collection, env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();
            Value index = eval_expr(expr->as.index_expr.index, env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();

            if (collection.type == VAL_ARRAY) {
                if (index.type != VAL_NUMBER) {
                    *result = exec_error("array index must be a number");
                    return make_none_value();
                }
                if (index.as.number < 0 ||
                    index.as.number >= collection.as.array.count) {
                    *result = exec_error("array index out of bounds");
                    return make_none_value();
                }
                if (index.as.number != (double)(int)index.as.number) {
                    *result = exec_error("array index must be an integer");
                    return make_none_value();
                }
                int i = (int)index.as.number;
                return collection.as.array.items[i];
            } else if (collection.type == VAL_STRING) {
                if (index.type != VAL_NUMBER) {
                    *result = exec_error("string index must be a number");
                    return make_none_value();
                }
                size_t slen = strlen(collection.as.string);
                if (index.as.number < 0 || index.as.number >= slen) {
                    *result = exec_error("string index out of bounds");
                    return make_none_value();
                }
                if (index.as.number != (double)(int)index.as.number) {
                    *result = exec_error("string index must be an integer");
                    return make_none_value();
                }
                char ch_str[2] = { collection.as.string[(int)index.as.number], '\0' };
                return make_string_value(ch_str);
            }

            *result = exec_error("cannot index a non-collection value");
            return make_none_value();
        }

        case EXPR_BINARY: return eval_binary(expr, env, result);
        case EXPR_UNARY:  return eval_unary(expr, env, result);
        case EXPR_CALL:   return eval_call(expr, env, result);
    }
    *result = exec_error("unknown expression type");
    return make_none_value();
}

static ExecResult exec_block(StmtList block, Environment *env) {
    for (int i = 0; i < block.count; i++) {
        ExecResult r = exec_stmt(block.items[i], env);
        if (r.signal != SIGNAL_NONE) return r;
    }
    return exec_none();
}

static ExecResult exec_stmt(Stmt *stmt, Environment *env) {
    ExecResult result = exec_none();

    switch (stmt->type) {
        case STMT_VAR_DECL: {
            Value v = eval_expr(stmt->as.var_decl.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            env_declare(env, stmt->as.var_decl.name, v, stmt->as.var_decl.is_global);
            return exec_none();
        }

        case STMT_ASSIGN: {
            Value v = eval_expr(stmt->as.assign.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            if (!env_set(env, stmt->as.assign.name, v)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "cannot assign to undeclared variable '%s'", stmt->as.assign.name);
                return exec_error(msg);
            }
            return exec_none();
        }

        case STMT_IF: {
            Value cond = eval_expr(stmt->as.if_stmt.condition, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            if (value_is_truthy(cond)) {
                env_push_scope(env);
                ExecResult r = exec_block(stmt->as.if_stmt.then_branch, env);
                env_pop_scope(env);
                return r;
            } else if (stmt->as.if_stmt.has_else) {
                env_push_scope(env);
                ExecResult r = exec_block(stmt->as.if_stmt.else_branch, env);
                env_pop_scope(env);
                return r;
            }
            return exec_none();
        }

        case STMT_LOOP_TILL: {
            for (;;) {
                Value cond = eval_expr(stmt->as.loop_till.condition, env, &result);
                if (result.signal != SIGNAL_NONE) return result;
                if (value_is_truthy(cond)) break;
                env_push_scope(env);
                ExecResult r = exec_block(stmt->as.loop_till.body, env);
                env_pop_scope(env);
                if (r.signal != SIGNAL_NONE) return r;
            }
            return exec_none();
        }

        case STMT_LOOP_RANGE: {
            Value from = eval_expr(stmt->as.loop_range.from, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            Value to = eval_expr(stmt->as.loop_range.to, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            if (from.type != VAL_NUMBER || to.type != VAL_NUMBER) {
                return exec_error("'loop from...to' requires numeric bounds");
            }
            for (double i = from.as.number; i <= to.as.number; i += 1) {
                env_push_scope(env);
                env_declare(env, stmt->as.loop_range.var_name, make_number_value(i), 0);
                ExecResult r = exec_block(stmt->as.loop_range.body, env);
                env_pop_scope(env);
                if (r.signal != SIGNAL_NONE) return r;
            }
            return exec_none();
        }

        case STMT_LOOP_THROUGH: {
            Value collection = eval_expr(stmt->as.loop_through.collection, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            if (collection.type == VAL_ARRAY) {
                for (int i = 0; i < collection.as.array.count; i++) {
                    env_push_scope(env);
                    env_declare(env, stmt->as.loop_through.item_name, collection.as.array.items[i], 0);
                    ExecResult r = exec_block(stmt->as.loop_through.body, env);
                    env_pop_scope(env);
                    if (r.signal != SIGNAL_NONE) return r;
                }
            } else if (collection.type == VAL_STRING) {
                size_t slen = strlen(collection.as.string);
                for (size_t i = 0; i < slen; i++) {
                    char ch_str[2] = { collection.as.string[i], '\0' };
                    env_push_scope(env);
                    env_declare(env, stmt->as.loop_through.item_name, make_string_value(ch_str), 0);
                    ExecResult r = exec_block(stmt->as.loop_through.body, env);
                    env_pop_scope(env);
                    if (r.signal != SIGNAL_NONE) return r;
                }
            } else {
                return exec_error("'loop through' requires an array or string");
            }
            return exec_none();
        }

        case STMT_FUNC_DECL:
            register_function(stmt->as.func_decl.name, stmt->as.func_decl.params, stmt->as.func_decl.body);
            return exec_none();

        case STMT_RETURN: {
            if (stmt->as.return_stmt.value == NULL) {
                return exec_return(make_none_value());
            }
            Value v = eval_expr(stmt->as.return_stmt.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            return exec_return(v);
        }

        case STMT_OUTPUT: {
            Value v = eval_expr(stmt->as.output_stmt.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            char *s = value_to_display_string(v);
            printf("%s\n", s);
            free(s);
            return exec_none();
        }

        case STMT_TRY_CATCH: {
            env_push_scope(env);
            ExecResult r = exec_block(stmt->as.try_catch.try_body, env);
            env_pop_scope(env);

            if (r.signal == SIGNAL_ERROR) {
                env_push_scope(env);
                env_declare(env, stmt->as.try_catch.error_name, r.value, 0);
                ExecResult catch_result = exec_block(stmt->as.try_catch.catch_body, env);
                env_pop_scope(env);
                return catch_result;
            }
            return r;
        }

        case STMT_EXPR: {
            eval_expr(stmt->as.expr_stmt.expr, env, &result);
            return result;
        }
    }

    return exec_error("unknown statement type");
}

void interpret_program(StmtList program) {
    Environment env;
    env_init(&env);

    for (int i = 0; i < program.count; i++) {
        if (program.items[i]->type == STMT_FUNC_DECL) {
            register_function(program.items[i]->as.func_decl.name,
                               program.items[i]->as.func_decl.params,
                               program.items[i]->as.func_decl.body);
        }
    }

    for (int i = 0; i < program.count; i++) {
        if (program.items[i]->type != STMT_FUNC_DECL) {
            ExecResult top_result = exec_stmt(program.items[i], &env);
            if (top_result.signal == SIGNAL_ERROR) {
                fprintf(stderr, "Uncaught error: %s\n", top_result.error_message);
                exit(1);
            }
            if (top_result.signal == SIGNAL_RETURN) {
                fprintf(stderr, "Error: return is only valid inside a function.\n");
                exit(1);
            }
        }
    }

    FuncDef *main_fn = find_function("main");
    if (!main_fn) {
        fprintf(stderr, "Error: no 'ft main()' function found.\n");
        exit(1);
    }

    ExecResult result = exec_none();
    call_user_function(main_fn, NULL, 0, &env, &result);

    if (result.signal == SIGNAL_ERROR) {
        fprintf(stderr, "Uncaught error: %s\n", result.error_message);
        exit(1);
    }

    env_free(&env);
    clear_functions();
    free_stmt_list(&program);
    value_cleanup();
}