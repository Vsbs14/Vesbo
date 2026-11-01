#include "../include/interpreter.h"
#include "../include/environment.h"
#include "../include/value.h"
#include "../include/builtins.h"
#include "../include/module.h"
#include "../include/codegen.h"
#include "../include/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    char *name;
    StringList params;
    StmtList body;
    Stmt *decl_stmt;
} FuncDef;

static FuncDef *functions = NULL;
static int func_count = 0;
static int func_capacity = 0;

static StructDef **interp_structs = NULL;
static Stmt **interp_struct_stmts = NULL;
static int interp_struct_count = 0;
static int interp_struct_cap = 0;

static StmtList *interp_imported_asts = NULL;
static int interp_imported_ast_count = 0;
static int interp_imported_ast_cap = 0;

static void retain_interp_imported_ast(StmtList ast) {
    if (interp_imported_ast_count >= interp_imported_ast_cap) {
        int new_cap = interp_imported_ast_cap == 0 ? 8 : interp_imported_ast_cap * 2;
        interp_imported_asts = realloc(interp_imported_asts, (size_t)new_cap * sizeof(StmtList));
        interp_imported_ast_cap = new_cap;
    }
    interp_imported_asts[interp_imported_ast_count++] = ast;
}

/* Call stack for file:line:col stack traces in interpreter */
typedef struct {
    const char *func_name;
    const char *call_file;
    int call_line;
    int call_col;
} InterpFrame;

#define MAX_INTERP_FRAMES 1024
static InterpFrame interp_frames[MAX_INTERP_FRAMES];
static int interp_frame_sp = 0;

static const char *cur_loc_file = "<script>";
static int cur_loc_line = 1;
static int cur_loc_col = 1;

/* Lazy bytecode VM used to execute coroutines with isolated stacks in interpreter mode */
static CodeGen *coro_codegen = NULL;
static VM *coro_vm = NULL;

static char *copy_string(const char *source) {
    if (!source) return NULL;
    size_t length = strlen(source);
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, source, length + 1);
    return copy;
}

static void invalidate_coro_vm(void) {
    if (coro_vm) {
        vm_free(coro_vm);
        coro_vm = NULL;
    }
    if (coro_codegen) {
        codegen_free(coro_codegen);
        coro_codegen = NULL;
    }
}

static void register_function_stmt(Stmt *stmt) {
    const char *name = stmt->as.func_decl.name;
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            functions[i].params = stmt->as.func_decl.params;
            functions[i].body = stmt->as.func_decl.body;
            functions[i].decl_stmt = stmt;
            invalidate_coro_vm();
            return;
        }
    }
    if (func_count >= func_capacity) {
        func_capacity = func_capacity == 0 ? 8 : func_capacity * 2;
        functions = realloc(functions, sizeof(FuncDef) * (size_t)func_capacity);
    }
    functions[func_count].name = copy_string(name);
    functions[func_count].params = stmt->as.func_decl.params;
    functions[func_count].body = stmt->as.func_decl.body;
    functions[func_count].decl_stmt = stmt;
    func_count++;
    invalidate_coro_vm();
}

static FuncDef *find_function(const char *name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) return &functions[i];
    }
    return NULL;
}

static StructDef *find_interp_struct(const char *name) {
    for (int i = 0; i < interp_struct_count; i++) {
        if (strcmp(interp_structs[i]->name, name) == 0) {
            return interp_structs[i];
        }
    }
    return NULL;
}

static void clear_functions(void) {
    for (int i = 0; i < func_count; i++) free(functions[i].name);
    free(functions);
    functions = NULL;
    func_count = 0;
    func_capacity = 0;

    free(interp_structs);
    free(interp_struct_stmts);
    interp_structs = NULL;
    interp_struct_stmts = NULL;
    interp_struct_count = 0;
    interp_struct_cap = 0;

    invalidate_coro_vm();

    for (int i = 0; i < interp_imported_ast_count; i++) {
        free_stmt_list(&interp_imported_asts[i]);
    }
    free(interp_imported_asts);
    interp_imported_asts = NULL;
    interp_imported_ast_count = 0;
    interp_imported_ast_cap = 0;
}

static void build_interp_stack_trace(const char *message) {
    char buf[4096];
    size_t cap = sizeof(buf);
    size_t pos = 0;

    const char *cur_fn = (interp_frame_sp > 0) ? interp_frames[interp_frame_sp - 1].func_name : "<top-level>";
    const char *file = cur_loc_file ? cur_loc_file : "<script>";

    int written = snprintf(buf + pos, cap - pos,
                           "Runtime error: %s\n  at %s (%s:%d:%d)",
                           message, cur_fn, file, cur_loc_line, cur_loc_col);
    if (written > 0 && (size_t)written < cap - pos) pos += (size_t)written;

    for (int i = interp_frame_sp - 1; i >= 0; i--) {
        const char *caller_fn = (i > 0) ? interp_frames[i - 1].func_name : "<top-level>";
        const char *cfile = interp_frames[i].call_file ? interp_frames[i].call_file : "<script>";
        written = snprintf(buf + pos, cap - pos,
                           "\n  at %s (%s:%d:%d)",
                           caller_fn, cfile, interp_frames[i].call_line, interp_frames[i].call_col);
        if (written > 0 && (size_t)written < cap - pos) pos += (size_t)written;
    }

    vesbo_set_last_stack_trace(buf);
}

typedef enum {
    SIGNAL_NONE,
    SIGNAL_RETURN,
    SIGNAL_BREAK,
    SIGNAL_CONTINUE,
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

static ExecResult exec_break(void) {
    ExecResult r;
    r.signal = SIGNAL_BREAK;
    r.value = make_none_value();
    r.error_message = NULL;
    return r;
}

static ExecResult exec_continue(void) {
    ExecResult r;
    r.signal = SIGNAL_CONTINUE;
    r.value = make_none_value();
    r.error_message = NULL;
    return r;
}

static ExecResult exec_error(const char *message) {
    build_interp_stack_trace(message);
    ExecResult r;
    r.signal = SIGNAL_ERROR;
    r.value = make_string_value(message);
    r.error_message = copy_string(message);
    return r;
}

static Value eval_expr(Expr *expr, Environment *env, ExecResult *result);
static ExecResult exec_stmt(Stmt *stmt, Environment *env);
static ExecResult exec_block(StmtList block, Environment *env);

static void register_struct_stmt(Stmt *stmt, Environment *env, ExecResult *result) {
    const char *name = stmt->as.struct_decl.name;
    int fcount = stmt->as.struct_decl.field_names.count;
    StructDef *def = struct_def_new(name, fcount);
    for (int i = 0; i < fcount; i++) {
        const char *fname = stmt->as.struct_decl.field_names.items[i];
        def->field_names[i] = intern_string(fname, strlen(fname));
        Expr *dexpr = (i < stmt->as.struct_decl.field_defaults.count)
                      ? stmt->as.struct_decl.field_defaults.items[i] : NULL;
        def->field_default_exprs[i] = dexpr;
        if (dexpr && env && result) {
            def->field_defaults[i] = eval_expr(dexpr, env, result);
        } else {
            def->field_defaults[i] = make_none_value();
        }
    }
    for (int i = 0; i < stmt->as.struct_decl.methods.count; i++) {
        Stmt *m = stmt->as.struct_decl.methods.items[i];
        if (m->type == STMT_FUNC_DECL) {
            struct_def_add_method(def, m->as.func_decl.name, -1, m);
        }
    }
    gc_register_struct_def(def);

    for (int i = 0; i < interp_struct_count; i++) {
        if (strcmp(interp_structs[i]->name, name) == 0) {
            interp_structs[i] = def;
            interp_struct_stmts[i] = stmt;
            invalidate_coro_vm();
            return;
        }
    }
    if (interp_struct_count >= interp_struct_cap) {
        interp_struct_cap = interp_struct_cap == 0 ? 8 : interp_struct_cap * 2;
        interp_structs = realloc(interp_structs, (size_t)interp_struct_cap * sizeof(StructDef *));
        interp_struct_stmts = realloc(interp_struct_stmts, (size_t)interp_struct_cap * sizeof(Stmt *));
    }
    interp_structs[interp_struct_count] = def;
    interp_struct_stmts[interp_struct_count] = stmt;
    interp_struct_count++;
    invalidate_coro_vm();
}

static VM *ensure_coro_vm(Environment *env) {
    if (!coro_vm) {
        coro_codegen = codegen_new();
        int total_stmts = interp_struct_count + func_count;
        Stmt **stmts = malloc((size_t)(total_stmts > 0 ? total_stmts : 1) * sizeof(Stmt *));
        int idx = 0;
        for (int i = 0; i < interp_struct_count; i++) {
            if (interp_struct_stmts[i]) stmts[idx++] = interp_struct_stmts[i];
        }
        for (int i = 0; i < func_count; i++) {
            /* Skip 'main' so codegen_compile doesn't auto-invoke main at chunk end */
            if (functions[i].decl_stmt && strcmp(functions[i].name, "main") != 0) {
                stmts[idx++] = functions[i].decl_stmt;
            }
        }
        Chunk *chunk = codegen_compile(coro_codegen, stmts, idx);
        free(stmts);
        /* Bind interpreter StructDefs to chunk structs so methods work across interpreter <-> coroutine boundary */
        for (int i = 0; i < interp_struct_count; i++) {
            StructDef *idef = interp_structs[i];
            int cidx = chunk_find_struct(chunk, idef->name);
            if (cidx >= 0) {
                chunk->structs[cidx].runtime_def = idef;
                size_t prefix_len = strlen(idef->name);
                for (size_t f = 0; f < chunk->functions_len; f++) {
                    const char *fn_name = chunk->functions[f].name;
                    if (strncmp(fn_name, idef->name, prefix_len) == 0 && fn_name[prefix_len] == '.') {
                        struct_def_add_method(idef, fn_name + prefix_len + 1, (int)f, NULL);
                    }
                }
            }
        }
        coro_vm = vm_new(chunk);
    }
    if (env && coro_vm) {
        /* Sync global variables from interpreter Environment (scopes[0]) into coro_vm */
        Scope *gscope = &env->scopes[0];
        for (int i = 0; i < gscope->count; i++) {
            vm_set_global_by_name(coro_vm, gscope->names[i], gscope->values[i]);
        }
    }
    return coro_vm;
}

static void sync_coro_vm_to_env(VM *vm, Environment *env) {
    if (!vm || !vm->chunk || !env) return;
    for (size_t i = 0; i < vm->chunk->globals_len; i++) {
        int defined = 0;
        Value v = vm_get_global_slot(vm, (uint32_t)i, &defined);
        if (defined) {
            const char *gname = vm->chunk->global_names[i];
            if (!env_set(env, gname, v)) {
                env_declare(env, gname, v, 1);
            }
        }
    }
}

static Value eval_binary(Expr *expr, Environment *env, ExecResult *result) {
    Value left = eval_expr(expr->as.binary.left, env, result);
    if (result->signal != SIGNAL_NONE) return make_none_value();
    gc_push_root(left);

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

    if (expr->file) cur_loc_file = expr->file;
    if (expr->line > 0) cur_loc_line = expr->line;
    if (expr->column > 0) cur_loc_col = expr->column;

    switch (expr->as.binary.op) {
        case BIN_ADD:
            if (left.type == VAL_STRING || right.type == VAL_STRING) {
                char *ls = value_to_display_string(left);
                char *rs = value_to_display_string(right);
                size_t llen = strlen(ls);
                size_t rlen = strlen(rs);
                char *combined = malloc(llen + rlen + 1);
                memcpy(combined, ls, llen);
                memcpy(combined + llen, rs, rlen + 1);
                Value v = make_string_value_len(combined, llen + rlen);
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

    if (expr->file) cur_loc_file = expr->file;
    if (expr->line > 0) cur_loc_line = expr->line;
    if (expr->column > 0) cur_loc_col = expr->column;

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

    const char *call_file = cur_loc_file;
    int call_line = cur_loc_line;
    int call_col = cur_loc_col;

    if (interp_frame_sp < MAX_INTERP_FRAMES) {
        interp_frames[interp_frame_sp].func_name = fn->name;
        interp_frames[interp_frame_sp].call_file = call_file;
        interp_frames[interp_frame_sp].call_line = call_line;
        interp_frames[interp_frame_sp].call_col = call_col;
        interp_frame_sp++;
    }

    env_push_scope(env);
    for (int i = 0; i < arg_count; i++) {
        env_declare(env, fn->params.items[i], args[i], 0);
    }

    ExecResult body_result = exec_block(fn->body, env);
    env_pop_scope(env);

    if (interp_frame_sp > 0) interp_frame_sp--;
    cur_loc_file = call_file;
    cur_loc_line = call_line;
    cur_loc_col = call_col;

    if (body_result.signal == SIGNAL_ERROR) {
        *result = body_result;
        return make_none_value();
    }
    if (body_result.signal == SIGNAL_BREAK || body_result.signal == SIGNAL_CONTINUE) {
        *result = exec_error("'break' or 'continue' outside of loop");
        return make_none_value();
    }
    if (body_result.signal == SIGNAL_RETURN) {
        return body_result.value;
    }
    return make_none_value();
}

static Value eval_call(Expr *expr, Environment *env, ExecResult *result) {
    const char *name = expr->as.call.callee_name;
    int arg_count = expr->as.call.args.count;

    /* Special handling for coroutine(fn, args...) / spawn(fn, args...) */
    if (strcmp(name, "coroutine") == 0 || strcmp(name, "spawn") == 0) {
        if (expr->file) cur_loc_file = expr->file;
        if (expr->line > 0) cur_loc_line = expr->line;
        if (expr->column > 0) cur_loc_col = expr->column;
        if (arg_count < 1) {
            *result = exec_error("coroutine() expects at least 1 argument (function)");
            return make_none_value();
        }
        Expr *first = expr->as.call.args.items[0];
        const char *fn_name = NULL;
        Value first_val = make_none_value();
        Value dummy;
        if (first->type == EXPR_IDENT && !env_get(env, first->as.ident_name, &dummy) && find_function(first->as.ident_name)) {
            fn_name = first->as.ident_name;
        } else {
            first_val = eval_expr(first, env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();
            if (first_val.type == VAL_STRING) {
                fn_name = first_val.as.string;
            }
        }
        if (expr->file) cur_loc_file = expr->file;
        if (expr->line > 0) cur_loc_line = expr->line;
        if (expr->column > 0) cur_loc_col = expr->column;
        if (!fn_name || !find_function(fn_name)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "undefined coroutine function '%s'", fn_name ? fn_name : "<invalid>");
            *result = exec_error(msg);
            return make_none_value();
        }
        int init_argc = arg_count - 1;
        Value *init_args = init_argc > 0 ? malloc((size_t)init_argc * sizeof(Value)) : NULL;
        for (int i = 0; i < init_argc; i++) {
            init_args[i] = eval_expr(expr->as.call.args.items[i + 1], env, result);
            if (result->signal != SIGNAL_NONE) {
                if (init_args) free(init_args);
                return make_none_value();
            }
            gc_push_root(init_args[i]);
        }
        Value co_val = make_coroutine_value(-1, fn_name, init_args, init_argc);
        if (init_args) free(init_args);
        return co_val;
    }

    /* Special handling for resume(co [, val]) */
    if (strcmp(name, "resume") == 0) {
        if (expr->file) cur_loc_file = expr->file;
        if (expr->line > 0) cur_loc_line = expr->line;
        if (expr->column > 0) cur_loc_col = expr->column;
        if (arg_count < 1 || arg_count > 2) {
            *result = exec_error("resume() expects 1 or 2 arguments (coroutine [, value])");
            return make_none_value();
        }
        Value co_val = eval_expr(expr->as.call.args.items[0], env, result);
        if (result->signal != SIGNAL_NONE) return make_none_value();
        Value send_val = make_none_value();
        int has_send = 0;
        if (arg_count == 2) {
            send_val = eval_expr(expr->as.call.args.items[1], env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();
            has_send = 1;
        }
        if (expr->file) cur_loc_file = expr->file;
        if (expr->line > 0) cur_loc_line = expr->line;
        if (expr->column > 0) cur_loc_col = expr->column;
        if (co_val.type != VAL_COROUTINE || !co_val.as.coroutine) {
            *result = exec_error("resume() first argument must be a coroutine");
            return make_none_value();
        }
        VM *vm = ensure_coro_vm(env);
        Value out = make_none_value();
        int status = vm_resume_coroutine(vm, co_val.as.coroutine, has_send, send_val, &out);
        sync_coro_vm_to_env(vm, env);
        if (status < 0) {
            char *saved_trace = copy_string(vesbo_get_last_stack_trace());
            *result = exec_error(vm->error_message[0] ? vm->error_message : "coroutine error");
            /* If the error happened inside the coroutine (not at the resume() boundary before entering),
             * combine the coroutine VM frames with the interpreter's caller frames. */
            if (saved_trace && saved_trace[0] && strstr(saved_trace, "(<script>:0:0)") == NULL) {
                char combined[4096];
                size_t cap = sizeof(combined);
                size_t pos = 0;
                int w = snprintf(combined, cap, "%s", saved_trace);
                if (w > 0 && (size_t)w < cap) pos = (size_t)w;
                const char *cur_fn = (interp_frame_sp > 0) ? interp_frames[interp_frame_sp - 1].func_name : "<top-level>";
                const char *file = cur_loc_file ? cur_loc_file : "<script>";
                w = snprintf(combined + pos, cap - pos, "\n  at %s (%s:%d:%d)",
                             cur_fn, file, cur_loc_line, cur_loc_col);
                if (w > 0 && (size_t)w < cap - pos) pos += (size_t)w;
                for (int i = interp_frame_sp - 1; i >= 0; i--) {
                    const char *caller_fn = (i > 0) ? interp_frames[i - 1].func_name : "<top-level>";
                    const char *cfile = interp_frames[i].call_file ? interp_frames[i].call_file : "<script>";
                    w = snprintf(combined + pos, cap - pos, "\n  at %s (%s:%d:%d)",
                                 caller_fn, cfile, interp_frames[i].call_line, interp_frames[i].call_col);
                    if (w > 0 && (size_t)w < cap - pos) pos += (size_t)w;
                }
                vesbo_set_last_stack_trace(combined);
            }
            free(saved_trace);
            return make_none_value();
        }
        return out;
    }

    Value *args = malloc(sizeof(Value) * (size_t)(arg_count > 0 ? arg_count : 1));
    for (int i = 0; i < arg_count; i++) {
        args[i] = eval_expr(expr->as.call.args.items[i], env, result);
        if (result->signal != SIGNAL_NONE) { free(args); return make_none_value(); }
        gc_push_root(args[i]);
    }

    if (expr->file) cur_loc_file = expr->file;
    if (expr->line > 0) cur_loc_line = expr->line;
    if (expr->column > 0) cur_loc_col = expr->column;

    /* Check struct constructor */
    StructDef *sdef = find_interp_struct(name);
    if (sdef) {
        if (arg_count > sdef->field_count) {
            char msg[256];
            snprintf(msg, sizeof(msg), "struct '%s' expects at most %d field argument(s), got %d",
                     sdef->name, sdef->field_count, arg_count);
            *result = exec_error(msg);
            free(args);
            return make_none_value();
        }
        Value inst = make_instance_value(sdef);
        gc_push_root(inst);
        for (int i = 0; i < arg_count; i++) {
            inst.as.instance->fields[i] = args[i];
        }
        for (int i = arg_count; i < sdef->field_count; i++) {
            if (sdef->field_default_exprs && sdef->field_default_exprs[i]) {
                inst.as.instance->fields[i] = eval_expr((Expr *)sdef->field_default_exprs[i], env, result);
                if (result->signal != SIGNAL_NONE) { free(args); return make_none_value(); }
            }
        }
        free(args);
        return inst;
    }

    Value out;
    const char *builtin_error = NULL;
    int builtin_status = call_builtin(name, args, arg_count, &out, &builtin_error);
    if (builtin_status == 1) {
        free(args);
        return out;
    }
    if (builtin_status == -1) {
        *result = exec_error(builtin_error);
        free(args);
        return make_none_value();
    }

    FuncDef *fn = find_function(name);
    if (!fn) {
        char msg[256];
        snprintf(msg, sizeof(msg), "undefined function '%s'", name);
        *result = exec_error(msg);
        free(args);
        return make_none_value();
    }

    Value ret = call_user_function(fn, args, arg_count, env, result);
    free(args);
    return ret;
}

static Value eval_method_call(Expr *expr, Environment *env, ExecResult *result) {
    Value receiver = eval_expr(expr->as.method_call.object, env, result);
    if (result->signal != SIGNAL_NONE) return make_none_value();
    gc_push_root(receiver);

    int arg_count = expr->as.method_call.args.count;
    Value *args = malloc(sizeof(Value) * (size_t)(arg_count > 0 ? arg_count : 1));
    for (int i = 0; i < arg_count; i++) {
        args[i] = eval_expr(expr->as.method_call.args.items[i], env, result);
        if (result->signal != SIGNAL_NONE) { free(args); return make_none_value(); }
        gc_push_root(args[i]);
    }

    if (expr->file) cur_loc_file = expr->file;
    if (expr->line > 0) cur_loc_line = expr->line;
    if (expr->column > 0) cur_loc_col = expr->column;

    const char *mname = expr->as.method_call.method_name;
    if (receiver.type != VAL_STRUCT || !receiver.as.instance) {
        char msg[256];
        snprintf(msg, sizeof(msg), "cannot call method '%s' on %s value",
                 mname, value_type_name(receiver));
        *result = exec_error(msg);
        free(args);
        return make_none_value();
    }

    StructDef *sdef = receiver.as.instance->struct_def;
    StructMethod *m = struct_def_find_method_str(sdef, mname);
    if (!m || !m->ast_stmt) {
        char msg[256];
        snprintf(msg, sizeof(msg), "struct '%s' has no method '%s'", sdef->name, mname);
        *result = exec_error(msg);
        free(args);
        return make_none_value();
    }

    Stmt *mstmt = (Stmt *)m->ast_stmt;
    int expected_args = mstmt->as.func_decl.params.count > 0 ? mstmt->as.func_decl.params.count - 1 : 0;
    if (arg_count != expected_args) {
        char msg[256];
        snprintf(msg, sizeof(msg), "method '%s.%s' expects %d argument(s), got %d",
                 sdef->name, mname, expected_args, arg_count);
        *result = exec_error(msg);
        free(args);
        return make_none_value();
    }

    char full_name[256];
    snprintf(full_name, sizeof(full_name), "%s.%s", sdef->name, mname);
    const char *call_file = cur_loc_file;
    int call_line = cur_loc_line;
    int call_col = cur_loc_col;
    if (interp_frame_sp < MAX_INTERP_FRAMES) {
        interp_frames[interp_frame_sp].func_name = full_name;
        interp_frames[interp_frame_sp].call_file = call_file;
        interp_frames[interp_frame_sp].call_line = call_line;
        interp_frames[interp_frame_sp].call_col = call_col;
        interp_frame_sp++;
    }

    env_push_scope(env);
    env_declare(env, "self", receiver, 0);
    for (int i = 0; i < arg_count; i++) {
        env_declare(env, mstmt->as.func_decl.params.items[i + 1], args[i], 0);
    }
    free(args);

    ExecResult body_result = exec_block(mstmt->as.func_decl.body, env);
    env_pop_scope(env);

    if (interp_frame_sp > 0) interp_frame_sp--;
    cur_loc_file = call_file;
    cur_loc_line = call_line;
    cur_loc_col = call_col;

    if (body_result.signal == SIGNAL_ERROR) {
        *result = body_result;
        return make_none_value();
    }
    if (body_result.signal == SIGNAL_BREAK || body_result.signal == SIGNAL_CONTINUE) {
        *result = exec_error("'break' or 'continue' outside of loop");
        return make_none_value();
    }
    if (body_result.signal == SIGNAL_RETURN) {
        return body_result.value;
    }
    return make_none_value();
}

static Value eval_expr(Expr *expr, Environment *env, ExecResult *result) {
    if (!expr) return make_none_value();
    if (expr->file) cur_loc_file = expr->file;
    if (expr->line > 0) cur_loc_line = expr->line;
    if (expr->column > 0) cur_loc_col = expr->column;

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
            gc_push_root(arr);
            for (int i = 0; i < expr->as.array.elements.count; i++) {
                Value item = eval_expr(expr->as.array.elements.items[i], env, result);
                if (result->signal != SIGNAL_NONE) return make_none_value();
                value_array_push(arr.as.array, item);
            }
            return arr;
        }

        case EXPR_MAP: {
            Value map_val = make_map_value();
            gc_push_root(map_val);
            for (int i = 0; i < expr->as.map.entries.count; i++) {
                Value k = eval_expr(expr->as.map.entries.items[i].key, env, result);
                if (result->signal != SIGNAL_NONE) return make_none_value();
                gc_push_root(k);
                Value v = eval_expr(expr->as.map.entries.items[i].value, env, result);
                if (result->signal != SIGNAL_NONE) return make_none_value();

                if (k.type == VAL_STRING) {
                    value_map_set_obj(map_val.as.map, AS_OBJ_STRING(k), v);
                } else {
                    char *kstr = value_to_display_string(k);
                    value_map_set(map_val.as.map, kstr, v);
                    free(kstr);
                }
            }
            return map_val;
        }

        case EXPR_INDEX: {
            Value collection = eval_expr(expr->as.index_expr.collection, env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();
            gc_push_root(collection);
            Value index = eval_expr(expr->as.index_expr.index, env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();

            if (collection.type == VAL_ARRAY) {
                if (index.type != VAL_NUMBER) {
                    *result = exec_error("array index must be a number");
                    return make_none_value();
                }
                if (index.as.number < 0 ||
                    index.as.number >= collection.as.array->count) {
                    *result = exec_error("array index out of bounds");
                    return make_none_value();
                }
                if (index.as.number != (double)(int)index.as.number) {
                    *result = exec_error("array index must be an integer");
                    return make_none_value();
                }
                int i = (int)index.as.number;
                return collection.as.array->items[i];
            } else if (collection.type == VAL_STRING) {
                if (index.type != VAL_NUMBER) {
                    *result = exec_error("string index must be a number");
                    return make_none_value();
                }
                size_t slen = (size_t)AS_OBJ_STRING(collection)->length;
                if (index.as.number < 0 || index.as.number >= (double)slen) {
                    *result = exec_error("string index out of bounds");
                    return make_none_value();
                }
                if (index.as.number != (double)(int)index.as.number) {
                    *result = exec_error("string index must be an integer");
                    return make_none_value();
                }
                char ch_str[2] = { collection.as.string[(int)index.as.number], '\0' };
                return make_string_value_len(ch_str, 1);
            } else if (collection.type == VAL_MAP) {
                Value out_val = make_none_value();
                int found;
                if (index.type == VAL_STRING) {
                    found = value_map_get_obj(collection.as.map, AS_OBJ_STRING(index), &out_val);
                } else {
                    char *kstr = value_to_display_string(index);
                    found = value_map_get(collection.as.map, kstr, &out_val);
                    free(kstr);
                }
                if (!found) {
                    return make_none_value();
                }
                return out_val;
            }

            *result = exec_error("cannot index a non-collection value");
            return make_none_value();
        }

        case EXPR_GET_FIELD: {
            Value obj = eval_expr(expr->as.get_field.object, env, result);
            if (result->signal != SIGNAL_NONE) return make_none_value();
            const char *fname = expr->as.get_field.field_name;

            if (obj.type == VAL_STRUCT && obj.as.instance) {
                int slot = struct_def_find_field_str(obj.as.instance->struct_def, fname);
                if (slot < 0) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "struct '%s' has no field '%s'",
                             obj.as.instance->struct_def->name, fname);
                    *result = exec_error(msg);
                    return make_none_value();
                }
                return obj.as.instance->fields[slot];
            } else if (obj.type == VAL_MAP && obj.as.map) {
                Value out_val = make_none_value();
                if (value_map_get(obj.as.map, fname, &out_val)) {
                    return out_val;
                }
                return make_none_value();
            } else {
                char msg[256];
                snprintf(msg, sizeof(msg), "cannot access field '%s' on %s value",
                         fname, value_type_name(obj));
                *result = exec_error(msg);
                return make_none_value();
            }
        }

        case EXPR_METHOD_CALL:
            return eval_method_call(expr, env, result);

        case EXPR_YIELD:
            *result = exec_error("cannot yield from outside a coroutine");
            return make_none_value();

        case EXPR_BINARY: return eval_binary(expr, env, result);
        case EXPR_UNARY:  return eval_unary(expr, env, result);
        case EXPR_CALL:   return eval_call(expr, env, result);
    }
    *result = exec_error("unknown expression type");
    return make_none_value();
}

static ExecResult exec_import_file(const char *raw_path, Environment *env) {
    char *resolved = module_resolve_path(raw_path, module_get_current_file());
    if (!resolved) {
        char msg[256];
        snprintf(msg, sizeof(msg), "cannot find module '%s'", raw_path);
        return exec_error(msg);
    }

    if (module_is_imported(resolved)) {
        free(resolved);
        return exec_none();
    }
    module_mark_imported(resolved);

    StmtList mod_stmts;
    char *err = NULL;
    char prev_file[4096];
    const char *cur_file = module_get_current_file();
    strncpy(prev_file, cur_file ? cur_file : "", sizeof(prev_file) - 1);
    prev_file[sizeof(prev_file) - 1] = '\0';
    module_set_current_file(resolved);

    if (!module_parse_file(resolved, &mod_stmts, &err)) {
        module_set_current_file(prev_file);
        char msg[256];
        snprintf(msg, sizeof(msg), "error importing '%s': %s", raw_path, err ? err : "parse error");
        if (err) free(err);
        free(resolved);
        return exec_error(msg);
    }
    retain_interp_imported_ast(mod_stmts);

    /* 1. Register functions and structs in imported module (skip 'main') */
    for (int i = 0; i < mod_stmts.count; i++) {
        if (mod_stmts.items[i]->type == STMT_IMPORT) {
            ExecResult r = exec_import_file(mod_stmts.items[i]->as.import_stmt.path, env);
            if (r.signal != SIGNAL_NONE) {
                module_set_current_file(prev_file);
                free(resolved);
                return r;
            }
        } else if (mod_stmts.items[i]->type == STMT_FUNC_DECL) {
            if (strcmp(mod_stmts.items[i]->as.func_decl.name, "main") != 0) {
                register_function_stmt(mod_stmts.items[i]);
            }
        } else if (mod_stmts.items[i]->type == STMT_STRUCT_DECL) {
            ExecResult sr = exec_none();
            register_struct_stmt(mod_stmts.items[i], env, &sr);
            if (sr.signal != SIGNAL_NONE) {
                module_set_current_file(prev_file);
                free(resolved);
                return sr;
            }
        }
    }

    /* 2. Execute top-level initialization statements in imported module */
    for (int i = 0; i < mod_stmts.count; i++) {
        StmtType st = mod_stmts.items[i]->type;
        if (st != STMT_FUNC_DECL && st != STMT_STRUCT_DECL && st != STMT_IMPORT) {
            ExecResult r = exec_stmt(mod_stmts.items[i], env);
            if (r.signal != SIGNAL_NONE) {
                module_set_current_file(prev_file);
                free(resolved);
                return r;
            }
        }
    }

    module_set_current_file(prev_file);
    free(resolved);
    return exec_none();
}

static ExecResult exec_block(StmtList block, Environment *env) {
    for (int i = 0; i < block.count; i++) {
        ExecResult r = exec_stmt(block.items[i], env);
        if (r.signal != SIGNAL_NONE) return r;
    }
    return exec_none();
}

static ExecResult exec_stmt_inner(Stmt *stmt, Environment *env) {
    ExecResult result = exec_none();
    if (stmt->file) cur_loc_file = stmt->file;
    if (stmt->line > 0) cur_loc_line = stmt->line;
    if (stmt->column > 0) cur_loc_col = stmt->column;

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

        case STMT_INDEX_SET: {
            Value target;
            if (stmt->as.index_set.collection) {
                target = eval_expr(stmt->as.index_set.collection, env, &result);
                if (result.signal != SIGNAL_NONE) return result;
                gc_push_root(target);
            } else {
                if (!env_get(env, stmt->as.index_set.name, &target)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "cannot index assign to undeclared variable '%s'", stmt->as.index_set.name);
                    return exec_error(msg);
                }
            }
            Value index = eval_expr(stmt->as.index_set.index, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            gc_push_root(index);
            Value val = eval_expr(stmt->as.index_set.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;

            if (target.type == VAL_MAP) {
                if (index.type == VAL_STRING) {
                    value_map_set_obj(target.as.map, AS_OBJ_STRING(index), val);
                } else {
                    char *kstr = value_to_display_string(index);
                    value_map_set(target.as.map, kstr, val);
                    free(kstr);
                }
                return exec_none();
            } else if (target.type == VAL_ARRAY) {
                if (index.type != VAL_NUMBER) {
                    return exec_error("array index must be a number");
                }
                if (index.as.number < 0 || index.as.number >= target.as.array->count) {
                    return exec_error("array index out of bounds");
                }
                if (index.as.number != (double)(int)index.as.number) {
                    return exec_error("array index must be an integer");
                }
                int i = (int)index.as.number;
                target.as.array->items[i] = val;
                return exec_none();
            } else {
                return exec_error("cannot index set on non-collection value");
            }
        }

        case STMT_SET_FIELD: {
            Value obj = eval_expr(stmt->as.set_field.object, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            gc_push_root(obj);
            Value val = eval_expr(stmt->as.set_field.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            const char *fname = stmt->as.set_field.field_name;

            if (obj.type == VAL_STRUCT && obj.as.instance) {
                int slot = struct_def_find_field_str(obj.as.instance->struct_def, fname);
                if (slot < 0) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "struct '%s' has no field '%s'",
                             obj.as.instance->struct_def->name, fname);
                    return exec_error(msg);
                }
                obj.as.instance->fields[slot] = val;
                return exec_none();
            } else if (obj.type == VAL_MAP && obj.as.map) {
                value_map_set(obj.as.map, fname, val);
                return exec_none();
            } else {
                char msg[256];
                snprintf(msg, sizeof(msg), "cannot set field '%s' on %s value",
                         fname, value_type_name(obj));
                return exec_error(msg);
            }
        }

        case STMT_STRUCT_DECL: {
            register_struct_stmt(stmt, env, &result);
            return result;
        }

        case STMT_IMPORT:
            return exec_import_file(stmt->as.import_stmt.path, env);

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
                int cond_roots = gc_save_roots();
                Value cond = eval_expr(stmt->as.loop_till.condition, env, &result);
                int truthy = value_is_truthy(cond);
                gc_restore_roots(cond_roots);
                if (result.signal != SIGNAL_NONE) return result;
                if (truthy) break;
                env_push_scope(env);
                ExecResult r = exec_block(stmt->as.loop_till.body, env);
                env_pop_scope(env);
                if (r.signal == SIGNAL_BREAK) break;
                if (r.signal == SIGNAL_CONTINUE) continue;
                if (r.signal != SIGNAL_NONE) return r;
            }
            return exec_none();
        }

        case STMT_LOOP_WHILE: {
            for (;;) {
                int cond_roots = gc_save_roots();
                Value cond = eval_expr(stmt->as.loop_while.condition, env, &result);
                int truthy = value_is_truthy(cond);
                gc_restore_roots(cond_roots);
                if (result.signal != SIGNAL_NONE) return result;
                if (!truthy) break;
                env_push_scope(env);
                ExecResult r = exec_block(stmt->as.loop_while.body, env);
                env_pop_scope(env);
                if (r.signal == SIGNAL_BREAK) break;
                if (r.signal == SIGNAL_CONTINUE) continue;
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
                if (r.signal == SIGNAL_BREAK) break;
                if (r.signal == SIGNAL_CONTINUE) continue;
                if (r.signal != SIGNAL_NONE) return r;
            }
            return exec_none();
        }

        case STMT_LOOP_THROUGH: {
            Value collection = eval_expr(stmt->as.loop_through.collection, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            if (collection.type == VAL_ARRAY) {
                for (int i = 0; i < collection.as.array->count; i++) {
                    env_push_scope(env);
                    env_declare(env, stmt->as.loop_through.item_name, collection.as.array->items[i], 0);
                    ExecResult r = exec_block(stmt->as.loop_through.body, env);
                    env_pop_scope(env);
                    if (r.signal == SIGNAL_BREAK) break;
                    if (r.signal == SIGNAL_CONTINUE) continue;
                    if (r.signal != SIGNAL_NONE) return r;
                }
            } else if (collection.type == VAL_STRING) {
                size_t slen = (size_t)AS_OBJ_STRING(collection)->length;
                for (size_t i = 0; i < slen; i++) {
                    char ch_str[2] = { collection.as.string[i], '\0' };
                    env_push_scope(env);
                    env_declare(env, stmt->as.loop_through.item_name, make_string_value_len(ch_str, 1), 0);
                    ExecResult r = exec_block(stmt->as.loop_through.body, env);
                    env_pop_scope(env);
                    if (r.signal == SIGNAL_BREAK) break;
                    if (r.signal == SIGNAL_CONTINUE) continue;
                    if (r.signal != SIGNAL_NONE) return r;
                }
            } else {
                return exec_error("'loop through' requires an array or string");
            }
            return exec_none();
        }

        case STMT_FUNC_DECL:
            register_function_stmt(stmt);
            return exec_none();

        case STMT_RETURN: {
            if (stmt->as.return_stmt.value == NULL) {
                return exec_return(make_none_value());
            }
            Value v = eval_expr(stmt->as.return_stmt.value, env, &result);
            if (result.signal != SIGNAL_NONE) return result;
            return exec_return(v);
        }

        case STMT_BREAK:
            return exec_break();

        case STMT_CONTINUE:
            return exec_continue();

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
                if (r.error_message) free(r.error_message);
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

static ExecResult exec_stmt(Stmt *stmt, Environment *env) {
    int saved_roots = gc_save_roots();
    gc_count_instruction();
    ExecResult r = exec_stmt_inner(stmt, env);
    gc_restore_roots(saved_roots);
    if (r.signal != SIGNAL_NONE && value_as_obj(r.value) != NULL) {
        gc_push_root(r.value);
    }
    return r;
}

void interpret_program(StmtList program) {
    Environment env;
    env_init(&env);
    interp_frame_sp = 0;

    for (int i = 0; i < program.count; i++) {
        if (program.items[i]->type == STMT_IMPORT) {
            ExecResult top_result = exec_stmt(program.items[i], &env);
            if (top_result.signal == SIGNAL_ERROR) {
                const char *trace = vesbo_get_last_stack_trace();
                if (trace && trace[0] != '\0') {
                    fprintf(stderr, "%s\n", trace);
                } else {
                    fprintf(stderr, "Runtime error: %s\n", top_result.error_message);
                }
                if (top_result.error_message) free(top_result.error_message);
                exit(1);
            }
        } else if (program.items[i]->type == STMT_FUNC_DECL) {
            register_function_stmt(program.items[i]);
        } else if (program.items[i]->type == STMT_STRUCT_DECL) {
            ExecResult sr = exec_none();
            register_struct_stmt(program.items[i], &env, &sr);
            if (sr.signal == SIGNAL_ERROR) {
                const char *trace = vesbo_get_last_stack_trace();
                if (trace && trace[0] != '\0') {
                    fprintf(stderr, "%s\n", trace);
                } else {
                    fprintf(stderr, "Runtime error: %s\n", sr.error_message);
                }
                if (sr.error_message) free(sr.error_message);
                exit(1);
            }
        }
    }

    for (int i = 0; i < program.count; i++) {
        StmtType st = program.items[i]->type;
        if (st != STMT_FUNC_DECL && st != STMT_STRUCT_DECL && st != STMT_IMPORT) {
            ExecResult top_result = exec_stmt(program.items[i], &env);
            if (top_result.signal == SIGNAL_ERROR) {
                const char *trace = vesbo_get_last_stack_trace();
                if (trace && trace[0] != '\0') {
                    fprintf(stderr, "%s\n", trace);
                } else {
                    fprintf(stderr, "Runtime error: %s\n", top_result.error_message);
                }
                if (top_result.error_message) free(top_result.error_message);
                exit(1);
            }
            if (top_result.signal == SIGNAL_RETURN) {
                fprintf(stderr, "Error: return is only valid inside a function.\n");
                exit(1);
            }
            if (top_result.signal == SIGNAL_BREAK || top_result.signal == SIGNAL_CONTINUE) {
                fprintf(stderr, "Error: break/continue is only valid inside a loop.\n");
                exit(1);
            }
        }
    }

    FuncDef *main_fn = find_function("main");
    if (!main_fn) {
        env_free(&env);
        clear_functions();
        free_stmt_list(&program);
        value_cleanup();
        return;
    }

    if (main_fn->decl_stmt) {
        cur_loc_file = main_fn->decl_stmt->file ? main_fn->decl_stmt->file : "<script>";
        cur_loc_line = main_fn->decl_stmt->line;
        cur_loc_col = main_fn->decl_stmt->column;
    }

    ExecResult result = exec_none();
    call_user_function(main_fn, NULL, 0, &env, &result);

    if (result.signal == SIGNAL_ERROR) {
        const char *trace = vesbo_get_last_stack_trace();
        if (trace && trace[0] != '\0') {
            fprintf(stderr, "%s\n", trace);
        } else {
            fprintf(stderr, "Runtime error: %s\n", result.error_message);
        }
        if (result.error_message) free(result.error_message);
        exit(1);
    }

    env_free(&env);
    clear_functions();
    free_stmt_list(&program);
    value_cleanup();
}

void interpret_repl_program(StmtList program, Environment *env) {
    for (int i = 0; i < program.count; i++) {
        if (program.items[i]->type == STMT_IMPORT) {
            ExecResult r = exec_stmt(program.items[i], env);
            if (r.signal == SIGNAL_ERROR) {
                fprintf(stderr, "Runtime error: %s\n", r.error_message ? r.error_message : "");
                if (r.error_message) free(r.error_message);
                return;
            }
        } else if (program.items[i]->type == STMT_FUNC_DECL) {
            register_function_stmt(program.items[i]);
        } else if (program.items[i]->type == STMT_STRUCT_DECL) {
            ExecResult sr = exec_none();
            register_struct_stmt(program.items[i], env, &sr);
            if (sr.signal == SIGNAL_ERROR) {
                fprintf(stderr, "Runtime error: %s\n", sr.error_message ? sr.error_message : "");
                if (sr.error_message) free(sr.error_message);
                return;
            }
        }
    }

    for (int i = 0; i < program.count; i++) {
        Stmt *s = program.items[i];
        if (s->type == STMT_FUNC_DECL || s->type == STMT_STRUCT_DECL || s->type == STMT_IMPORT) continue;

        if (s->type == STMT_EXPR) {
            int saved_roots = gc_save_roots();
            ExecResult r = exec_none();
            Value v = eval_expr(s->as.expr_stmt.expr, env, &r);
            if (r.signal == SIGNAL_ERROR) {
                fprintf(stderr, "Runtime error: %s\n", r.error_message ? r.error_message : "");
                if (r.error_message) free(r.error_message);
                gc_restore_roots(saved_roots);
                return;
            }
            if (v.type != VAL_NONE) {
                char *disp = value_to_display_string(v);
                printf("%s\n", disp);
                free(disp);
            }
            gc_restore_roots(saved_roots);
        } else {
            ExecResult r = exec_stmt(s, env);
            if (r.signal == SIGNAL_ERROR) {
                fprintf(stderr, "Runtime error: %s\n", r.error_message ? r.error_message : "");
                if (r.error_message) free(r.error_message);
                return;
            }
            if (r.signal == SIGNAL_RETURN) {
                fprintf(stderr, "Error: return is only valid inside a function.\n");
                return;
            }
            if (r.signal == SIGNAL_BREAK || r.signal == SIGNAL_CONTINUE) {
                fprintf(stderr, "Error: break/continue is only valid inside a loop.\n");
                return;
            }
        }
    }
}

void interpret_cleanup(void) {
    clear_functions();
}