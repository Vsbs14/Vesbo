#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/codegen.h"
#include "../include/builtins.h"
#include "../include/module.h"

typedef struct {
    char **names;
    int count;
    int capacity;
} LocalVarList;

typedef struct LoopContext {
    size_t continue_target;
    int has_continue_target;
    size_t *continue_jumps;
    int continue_count;
    int continue_cap;
    size_t *break_jumps;
    int break_count;
    int break_cap;
    struct LoopContext *enclosing;
} LoopContext;

struct CodeGen {
    Chunk *chunk;
    LocalVarList locals;
    int in_function;
    Stmt **struct_decls;
    int struct_decl_count;
    int struct_decl_cap;
    StmtList *imported_asts;
    int imported_ast_count;
    int imported_ast_cap;
    LoopContext *current_loop;
};

static void loop_context_add_break(LoopContext *loop, size_t jump_addr) {
    if (!loop) return;
    if (loop->break_count >= loop->break_cap) {
        loop->break_cap = loop->break_cap == 0 ? 8 : loop->break_cap * 2;
        loop->break_jumps = realloc(loop->break_jumps, (size_t)loop->break_cap * sizeof(size_t));
    }
    loop->break_jumps[loop->break_count++] = jump_addr;
}

static void loop_context_add_continue(LoopContext *loop, size_t jump_addr) {
    if (!loop) return;
    if (loop->continue_count >= loop->continue_cap) {
        loop->continue_cap = loop->continue_cap == 0 ? 8 : loop->continue_cap * 2;
        loop->continue_jumps = realloc(loop->continue_jumps, (size_t)loop->continue_cap * sizeof(size_t));
    }
    loop->continue_jumps[loop->continue_count++] = jump_addr;
}

static void loop_context_patch_continues(LoopContext *loop, Chunk *chunk, size_t target) {
    if (!loop || !chunk) return;
    loop->continue_target = target;
    loop->has_continue_target = 1;
    for (int i = 0; i < loop->continue_count; i++) {
        chunk_patch_operand(chunk, loop->continue_jumps[i] + 1, (uint32_t)target);
    }
}

static void codegen_retain_ast(CodeGen *gen, StmtList ast) {
    if (gen->imported_ast_count >= gen->imported_ast_cap) {
        int new_cap = gen->imported_ast_cap == 0 ? 8 : gen->imported_ast_cap * 2;
        gen->imported_asts = realloc(gen->imported_asts, (size_t)new_cap * sizeof(StmtList));
        gen->imported_ast_cap = new_cap;
    }
    gen->imported_asts[gen->imported_ast_count++] = ast;
}

static void codegen_record_struct_decl(CodeGen *gen, Stmt *stmt) {
    if (!gen || !stmt || stmt->type != STMT_STRUCT_DECL) return;
    for (int i = 0; i < gen->struct_decl_count; i++) {
        if (strcmp(gen->struct_decls[i]->as.struct_decl.name, stmt->as.struct_decl.name) == 0) {
            gen->struct_decls[i] = stmt;
            return;
        }
    }
    if (gen->struct_decl_count >= gen->struct_decl_cap) {
        int new_cap = gen->struct_decl_cap == 0 ? 8 : gen->struct_decl_cap * 2;
        gen->struct_decls = realloc(gen->struct_decls, (size_t)new_cap * sizeof(Stmt *));
        gen->struct_decl_cap = new_cap;
    }
    gen->struct_decls[gen->struct_decl_count++] = stmt;
}

static Stmt *codegen_find_struct_decl(CodeGen *gen, const char *name) {
    if (!gen || !name) return NULL;
    for (int i = 0; i < gen->struct_decl_count; i++) {
        if (strcmp(gen->struct_decls[i]->as.struct_decl.name, name) == 0) {
            return gen->struct_decls[i];
        }
    }
    return NULL;
}

static void local_vars_init(LocalVarList *vars) {
    vars->names = malloc(32 * sizeof(char *));
    vars->capacity = 32;
    vars->count = 0;
}

static void local_vars_free(LocalVarList *vars) {
    for (int i = 0; i < vars->count; i++) {
        free(vars->names[i]);
    }
    free(vars->names);
}

static int local_vars_add(LocalVarList *vars, const char *name) {
    if (vars->count >= vars->capacity) {
        vars->capacity *= 2;
        vars->names = realloc(vars->names, (size_t)vars->capacity * sizeof(char *));
    }
    vars->names[vars->count] = malloc(strlen(name) + 1);
    strcpy(vars->names[vars->count], name);
    return vars->count++;
}

static int local_vars_find(LocalVarList *vars, const char *name) {
    for (int i = vars->count - 1; i >= 0; i--) {
        if (strcmp(vars->names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static void local_vars_clear(LocalVarList *vars) {
    for (int i = 0; i < vars->count; i++) {
        free(vars->names[i]);
    }
    vars->count = 0;
}

static void local_vars_pop(CodeGen *gen) {
    LocalVarList *vars = &gen->locals;
    if (vars->count == 0) return;
    vars->count--;
    free(vars->names[vars->count]);
    chunk_write_op(gen->chunk, OP_POP);
}

static Value eval_const_default(Expr *expr) {
    if (!expr) return make_none_value();
    switch (expr->type) {
        case EXPR_NUMBER: return make_number_value(expr->as.number_val);
        case EXPR_STRING: return make_string_value(expr->as.string_val);
        case EXPR_BOOL:   return make_bool_value(expr->as.bool_val);
        case EXPR_NONE:   return make_none_value();
        case EXPR_UNARY:
            if (expr->as.unary.op == UN_NEGATE && expr->as.unary.operand &&
                expr->as.unary.operand->type == EXPR_NUMBER) {
                return make_number_value(-expr->as.unary.operand->as.number_val);
            }
            return make_none_value();
        default:
            return make_none_value();
    }
}

static void register_struct_in_chunk(CodeGen *gen, Stmt *stmt) {
    Chunk *chunk = gen->chunk;
    codegen_record_struct_decl(gen, stmt);
    int fcount = stmt->as.struct_decl.field_names.count;
    char **fnames = stmt->as.struct_decl.field_names.items;
    Value *fdefaults = fcount > 0 ? malloc((size_t)fcount * sizeof(Value)) : NULL;
    for (int i = 0; i < fcount; i++) {
        Expr *def_expr = (i < stmt->as.struct_decl.field_defaults.count)
                         ? stmt->as.struct_decl.field_defaults.items[i] : NULL;
        fdefaults[i] = eval_const_default(def_expr);
    }
    chunk_add_struct(chunk, stmt->as.struct_decl.name, fcount, fnames, fdefaults);
    if (fdefaults) free(fdefaults);

    for (int i = 0; i < stmt->as.struct_decl.methods.count; i++) {
        Stmt *m = stmt->as.struct_decl.methods.items[i];
        if (m->type == STMT_FUNC_DECL) {
            char mname[512];
            snprintf(mname, sizeof(mname), "%s.%s", stmt->as.struct_decl.name, m->as.func_decl.name);
            if (chunk_find_function(chunk, mname) < 0) {
                chunk_add_function_loc(chunk, mname, 0, m->as.func_decl.params.count,
                                       m->file, m->line, m->column);
            }
        }
    }
}

/* Forward declarations */
static void codegen_stmt(CodeGen *gen, Stmt *stmt);
static void codegen_expr(CodeGen *gen, Expr *expr);
static void codegen_import(CodeGen *gen, const char *raw_path);

static void codegen_stmt_list(CodeGen *gen, StmtList *stmts) {
    for (int i = 0; i < stmts->count; i++) {
        codegen_stmt(gen, stmts->items[i]);
    }
}

static void codegen_expr(CodeGen *gen, Expr *expr) {
    if (!expr) return;
    chunk_set_location(gen->chunk, expr->file, expr->line, expr->column);

    switch (expr->type) {
        case EXPR_NUMBER:
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_number(gen->chunk, expr->as.number_val));
            break;

        case EXPR_STRING: {
            chunk_write_op(gen->chunk, OP_PUSH_STRING);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_string(gen->chunk, expr->as.string_val));
            break;
        }

        case EXPR_BOOL:
            if (expr->as.bool_val) {
                chunk_write_op(gen->chunk, OP_PUSH_TRUE);
            } else {
                chunk_write_op(gen->chunk, OP_PUSH_FALSE);
            }
            break;

        case EXPR_NONE:
            chunk_write_op(gen->chunk, OP_PUSH_NONE);
            break;

        case EXPR_IDENT: {
            int idx = local_vars_find(&gen->locals, expr->as.ident_name);
            if (idx >= 0) {
                chunk_write_op(gen->chunk, OP_GET_LOCAL);
                chunk_write_operand(gen->chunk, (uint32_t)idx);
            } else {
                chunk_write_op(gen->chunk, OP_GET_GLOBAL);
                chunk_write_operand(gen->chunk, chunk_resolve_global(gen->chunk, expr->as.ident_name));
            }
            break;
        }

        case EXPR_ARRAY: {
            for (int i = 0; i < expr->as.array.elements.count; i++) {
                codegen_expr(gen, expr->as.array.elements.items[i]);
            }
            chunk_write_op(gen->chunk, OP_PUSH_ARRAY);
            chunk_write_operand(gen->chunk, (uint32_t)expr->as.array.elements.count);
            break;
        }

        case EXPR_MAP: {
            for (int i = 0; i < expr->as.map.entries.count; i++) {
                codegen_expr(gen, expr->as.map.entries.items[i].key);
                codegen_expr(gen, expr->as.map.entries.items[i].value);
            }
            chunk_write_op(gen->chunk, OP_BUILD_MAP);
            chunk_write_operand(gen->chunk, (uint32_t)expr->as.map.entries.count);
            break;
        }

        case EXPR_INDEX:
            codegen_expr(gen, expr->as.index_expr.collection);
            codegen_expr(gen, expr->as.index_expr.index);
            chunk_write_op(gen->chunk, OP_INDEX);
            break;

        case EXPR_GET_FIELD:
            codegen_expr(gen, expr->as.get_field.object);
            chunk_write_op(gen->chunk, OP_GET_FIELD);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_string(gen->chunk, expr->as.get_field.field_name));
            break;

        case EXPR_METHOD_CALL: {
            codegen_expr(gen, expr->as.method_call.object);
            for (int i = 0; i < expr->as.method_call.args.count; i++) {
                codegen_expr(gen, expr->as.method_call.args.items[i]);
            }
            chunk_write_op(gen->chunk, OP_INVOKE);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_string(gen->chunk, expr->as.method_call.method_name));
            chunk_write_operand(gen->chunk, (uint32_t)expr->as.method_call.args.count);
            break;
        }

        case EXPR_YIELD:
            if (expr->as.yield_expr.value) {
                codegen_expr(gen, expr->as.yield_expr.value);
            } else {
                chunk_write_op(gen->chunk, OP_PUSH_NONE);
            }
            chunk_write_op(gen->chunk, OP_YIELD);
            break;

        case EXPR_BINARY: {
            if (expr->as.binary.op == BIN_AND) {
                codegen_expr(gen, expr->as.binary.left);
                size_t jump_false = gen->chunk->code_len;
                chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
                chunk_write_operand(gen->chunk, 0);

                codegen_expr(gen, expr->as.binary.right);
                chunk_write_op(gen->chunk, OP_NOT);
                chunk_write_op(gen->chunk, OP_NOT);

                size_t jump_end = gen->chunk->code_len;
                chunk_write_op(gen->chunk, OP_JUMP);
                chunk_write_operand(gen->chunk, 0);

                size_t false_target = gen->chunk->code_len;
                chunk_patch_operand(gen->chunk, jump_false + 1, (uint32_t)false_target);
                chunk_write_op(gen->chunk, OP_PUSH_FALSE);

                size_t end_target = gen->chunk->code_len;
                chunk_patch_operand(gen->chunk, jump_end + 1, (uint32_t)end_target);
                break;
            }
            if (expr->as.binary.op == BIN_OR) {
                codegen_expr(gen, expr->as.binary.left);
                chunk_write_op(gen->chunk, OP_NOT);
                size_t jump_true = gen->chunk->code_len;
                chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
                chunk_write_operand(gen->chunk, 0);

                codegen_expr(gen, expr->as.binary.right);
                chunk_write_op(gen->chunk, OP_NOT);
                chunk_write_op(gen->chunk, OP_NOT);

                size_t jump_end = gen->chunk->code_len;
                chunk_write_op(gen->chunk, OP_JUMP);
                chunk_write_operand(gen->chunk, 0);

                size_t true_target = gen->chunk->code_len;
                chunk_patch_operand(gen->chunk, jump_true + 1, (uint32_t)true_target);
                chunk_write_op(gen->chunk, OP_PUSH_TRUE);

                size_t end_target = gen->chunk->code_len;
                chunk_patch_operand(gen->chunk, jump_end + 1, (uint32_t)end_target);
                break;
            }

            codegen_expr(gen, expr->as.binary.left);
            codegen_expr(gen, expr->as.binary.right);

            switch (expr->as.binary.op) {
                case BIN_ADD: chunk_write_op(gen->chunk, OP_ADD); break;
                case BIN_SUB: chunk_write_op(gen->chunk, OP_SUBTRACT); break;
                case BIN_MUL: chunk_write_op(gen->chunk, OP_MULTIPLY); break;
                case BIN_DIV: chunk_write_op(gen->chunk, OP_DIVIDE); break;
                case BIN_MOD: chunk_write_op(gen->chunk, OP_MODULO); break;
                case BIN_EQUALS: chunk_write_op(gen->chunk, OP_EQUALS); break;
                case BIN_NOT_EQUALS: chunk_write_op(gen->chunk, OP_NOT_EQUALS); break;
                case BIN_LESS: chunk_write_op(gen->chunk, OP_LESS_THAN); break;
                case BIN_LESS_EQUAL: chunk_write_op(gen->chunk, OP_LESS_THAN_EQUAL); break;
                case BIN_GREATER: chunk_write_op(gen->chunk, OP_GREATER_THAN); break;
                case BIN_GREATER_EQUAL: chunk_write_op(gen->chunk, OP_GREATER_THAN_EQUAL); break;
                case BIN_AND: chunk_write_op(gen->chunk, OP_AND); break;
                case BIN_OR: chunk_write_op(gen->chunk, OP_OR); break;
            }
            break;
        }

        case EXPR_UNARY:
            codegen_expr(gen, expr->as.unary.operand);
            if (expr->as.unary.op == UN_NOT) {
                chunk_write_op(gen->chunk, OP_NOT);
            } else {
                chunk_write_op(gen->chunk, OP_NEGATE);
            }
            break;

        case EXPR_CALL: {
            const char *name = expr->as.call.callee_name;

            /* Special handling for coroutine creation: coroutine(fn, args...) / spawn(fn, args...) */
            if (strcmp(name, "coroutine") == 0 || strcmp(name, "spawn") == 0) {
                if (expr->as.call.args.count >= 1) {
                    Expr *first = expr->as.call.args.items[0];
                    int target_func_idx = -1;
                    if (first->type == EXPR_IDENT &&
                        local_vars_find(&gen->locals, first->as.ident_name) < 0 &&
                        chunk_find_global(gen->chunk, first->as.ident_name) < 0) {
                        target_func_idx = chunk_find_function(gen->chunk, first->as.ident_name);
                    } else if (first->type == EXPR_STRING) {
                        target_func_idx = chunk_find_function(gen->chunk, first->as.string_val);
                    }

                    if (target_func_idx >= 0) {
                        for (int i = 1; i < expr->as.call.args.count; i++) {
                            codegen_expr(gen, expr->as.call.args.items[i]);
                        }
                        chunk_write_op(gen->chunk, OP_COROUTINE);
                        chunk_write_operand(gen->chunk, (uint32_t)target_func_idx);
                        chunk_write_operand(gen->chunk, (uint32_t)(expr->as.call.args.count - 1));
                        break;
                    }
                }
                /* Dynamic function name expression in arg 0 */
                for (int i = 0; i < expr->as.call.args.count; i++) {
                    codegen_expr(gen, expr->as.call.args.items[i]);
                }
                chunk_write_op(gen->chunk, OP_COROUTINE);
                chunk_write_operand(gen->chunk, 0xFFFFFFFFu);
                chunk_write_operand(gen->chunk, (uint32_t)(expr->as.call.args.count > 0 ? expr->as.call.args.count - 1 : 0));
                break;
            }

            /* Special handling for coroutine resume: resume(co [, val]) */
            if (strcmp(name, "resume") == 0) {
                for (int i = 0; i < expr->as.call.args.count; i++) {
                    codegen_expr(gen, expr->as.call.args.items[i]);
                }
                chunk_write_op(gen->chunk, OP_RESUME);
                chunk_write_operand(gen->chunk, (uint32_t)expr->as.call.args.count);
                break;
            }

            for (int i = 0; i < expr->as.call.args.count; i++) {
                codegen_expr(gen, expr->as.call.args.items[i]);
            }

            /* Struct instantiation: StructName(arg1, arg2, ...) */
            int struct_idx = chunk_find_struct(gen->chunk, name);
            if (struct_idx >= 0) {
                int total_args = expr->as.call.args.count;
                Stmt *sdecl = codegen_find_struct_decl(gen, name);
                if (sdecl && total_args < sdecl->as.struct_decl.field_names.count) {
                    int fcount = sdecl->as.struct_decl.field_names.count;
                    for (int i = total_args; i < fcount; i++) {
                        Expr *def_expr = (i < sdecl->as.struct_decl.field_defaults.count)
                                         ? sdecl->as.struct_decl.field_defaults.items[i] : NULL;
                        if (def_expr) {
                            codegen_expr(gen, def_expr);
                        } else {
                            chunk_write_op(gen->chunk, OP_PUSH_NONE);
                        }
                    }
                    total_args = fcount;
                }
                chunk_set_location(gen->chunk, expr->file, expr->line, expr->column);
                chunk_write_op(gen->chunk, OP_NEW_STRUCT);
                chunk_write_operand(gen->chunk, (uint32_t)struct_idx);
                chunk_write_operand(gen->chunk, (uint32_t)total_args);
                break;
            }

            /* Check for builtins */
            if (strcmp(name, "output") == 0) {
                chunk_write_op(gen->chunk, OP_OUTPUT);
                chunk_write_op(gen->chunk, OP_PUSH_NONE);
            } else if (strcmp(name, "input") == 0) {
                chunk_write_op(gen->chunk, OP_INPUT);
            } else if (strcmp(name, "length") == 0) {
                chunk_write_op(gen->chunk, OP_LENGTH);
            } else if (strcmp(name, "lowercase") == 0) {
                chunk_write_op(gen->chunk, OP_LOWERCASE);
            } else if (strcmp(name, "trim") == 0) {
                chunk_write_op(gen->chunk, OP_TRIM);
            } else if (strcmp(name, "number") == 0) {
                chunk_write_op(gen->chunk, OP_NUMBER_CAST);
            } else if (strcmp(name, "uppercase") == 0) {
                chunk_write_op(gen->chunk, OP_UPPERCASE);
            } else if (strcmp(name, "string") == 0) {
                chunk_write_op(gen->chunk, OP_STRING_CAST);
            } else if (strcmp(name, "push") == 0) {
                chunk_write_op(gen->chunk, OP_PUSH_BACK);
            } else if (strcmp(name, "type_of") == 0) {
                chunk_write_op(gen->chunk, OP_TYPE_OF);
            } else {
                int func_idx = chunk_find_function(gen->chunk, name);
                if (func_idx >= 0 && !is_builtin(name)) {
                    chunk_write_op(gen->chunk, OP_CALL);
                    chunk_write_operand(gen->chunk, (uint32_t)func_idx);
                } else {
                    /* Built-in or host-registered native function (or unknown function reported at runtime) */
                    chunk_write_op(gen->chunk, OP_CALL_BUILTIN);
                    chunk_write_operand(gen->chunk, (uint32_t)chunk_add_string(gen->chunk, name));
                    chunk_write_operand(gen->chunk, (uint32_t)expr->as.call.args.count);
                }
            }
            break;
        }
    }
}

static void codegen_import(CodeGen *gen, const char *raw_path) {
    char *resolved = module_resolve_path(raw_path, module_get_current_file());
    if (!resolved) {
        fprintf(stderr, "Error: cannot find module '%s'\n", raw_path);
        return;
    }

    if (module_is_imported(resolved)) {
        free(resolved);
        return;
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
        fprintf(stderr, "Error importing '%s': %s\n", raw_path, err ? err : "parse error");
        if (err) free(err);
        free(resolved);
        return;
    }
    codegen_retain_ast(gen, mod_stmts);

    for (int i = 0; i < mod_stmts.count; i++) {
        if (mod_stmts.items[i]->type == STMT_FUNC_DECL) {
            if (strcmp(mod_stmts.items[i]->as.func_decl.name, "main") != 0) {
                codegen_stmt(gen, mod_stmts.items[i]);
            }
        } else {
            codegen_stmt(gen, mod_stmts.items[i]);
        }
    }

    module_set_current_file(prev_file);
    free(resolved);
}

static void codegen_stmt(CodeGen *gen, Stmt *stmt) {
    if (!stmt) return;
    chunk_set_location(gen->chunk, stmt->file, stmt->line, stmt->column);

    switch (stmt->type) {
        case STMT_VAR_DECL: {
            codegen_expr(gen, stmt->as.var_decl.value);
            if (stmt->as.var_decl.is_global || !gen->in_function) {
                chunk_write_op(gen->chunk, OP_SET_GLOBAL);
                chunk_write_operand(gen->chunk, chunk_resolve_global(gen->chunk, stmt->as.var_decl.name));
                chunk_write_op(gen->chunk, OP_POP);
            } else {
                int idx = local_vars_add(&gen->locals, stmt->as.var_decl.name);
                chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
                chunk_write_operand(gen->chunk, (uint32_t)idx);
            }
            break;
        }

        case STMT_ASSIGN: {
            codegen_expr(gen, stmt->as.assign.value);
            int idx = local_vars_find(&gen->locals, stmt->as.assign.name);
            if (idx >= 0) {
                chunk_write_op(gen->chunk, OP_SET_LOCAL);
                chunk_write_operand(gen->chunk, (uint32_t)idx);
            } else {
                chunk_write_op(gen->chunk, OP_SET_GLOBAL);
                chunk_write_operand(gen->chunk, chunk_resolve_global(gen->chunk, stmt->as.assign.name));
                chunk_write_op(gen->chunk, OP_POP);
            }
            break;
        }

        case STMT_INDEX_SET: {
            if (stmt->as.index_set.collection) {
                codegen_expr(gen, stmt->as.index_set.collection);
            } else {
                int idx = local_vars_find(&gen->locals, stmt->as.index_set.name);
                if (idx >= 0) {
                    chunk_write_op(gen->chunk, OP_GET_LOCAL);
                    chunk_write_operand(gen->chunk, (uint32_t)idx);
                } else {
                    chunk_write_op(gen->chunk, OP_GET_GLOBAL);
                    chunk_write_operand(gen->chunk, chunk_resolve_global(gen->chunk, stmt->as.index_set.name));
                }
            }
            codegen_expr(gen, stmt->as.index_set.index);
            codegen_expr(gen, stmt->as.index_set.value);
            chunk_write_op(gen->chunk, OP_INDEX_SET);
            break;
        }

        case STMT_SET_FIELD: {
            codegen_expr(gen, stmt->as.set_field.object);
            codegen_expr(gen, stmt->as.set_field.value);
            chunk_write_op(gen->chunk, OP_SET_FIELD);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_string(gen->chunk, stmt->as.set_field.field_name));
            break;
        }

        case STMT_STRUCT_DECL: {
            register_struct_in_chunk(gen, stmt);

            /* Compile struct methods out-of-line with implicit 'self' at local slot 0 */
            for (int m_idx = 0; m_idx < stmt->as.struct_decl.methods.count; m_idx++) {
                Stmt *m = stmt->as.struct_decl.methods.items[m_idx];
                if (m->type != STMT_FUNC_DECL) continue;

                chunk_set_location(gen->chunk, m->file, m->line, m->column);
                size_t skip_jump_addr = gen->chunk->code_len;
                chunk_write_op(gen->chunk, OP_JUMP);
                chunk_write_operand(gen->chunk, 0);

                uint32_t func_offset = (uint32_t)gen->chunk->code_len;
                char mname[512];
                snprintf(mname, sizeof(mname), "%s.%s", stmt->as.struct_decl.name, m->as.func_decl.name);
                int func_idx = chunk_find_function(gen->chunk, mname);
                if (func_idx >= 0) {
                    gen->chunk->functions[func_idx].code_offset = func_offset;
                    gen->chunk->functions[func_idx].param_count = m->as.func_decl.params.count;
                } else {
                    func_idx = chunk_add_function_loc(gen->chunk, mname, func_offset,
                                                      m->as.func_decl.params.count,
                                                      m->file, m->line, m->column);
                }

                if (func_idx >= 0) {
                    LocalVarList saved_locals = gen->locals;
                    int saved_in_func = gen->in_function;
                    local_vars_init(&gen->locals);
                    gen->in_function = 1;

                    /* Slot 0 is already 'self' from parser */
                    for (int p = 0; p < m->as.func_decl.params.count; p++) {
                        local_vars_add(&gen->locals, m->as.func_decl.params.items[p]);
                    }

                    codegen_stmt_list(gen, &m->as.func_decl.body);

                    chunk_write_op(gen->chunk, OP_PUSH_NONE);
                    chunk_write_op(gen->chunk, OP_RETURN);

                    local_vars_free(&gen->locals);
                    gen->locals = saved_locals;
                    gen->in_function = saved_in_func;
                }

                size_t after_method = gen->chunk->code_len;
                chunk_patch_operand(gen->chunk, skip_jump_addr + 1, (uint32_t)after_method);
            }
            break;
        }

        case STMT_IMPORT:
            codegen_import(gen, stmt->as.import_stmt.path);
            break;

        case STMT_IF: {
            codegen_expr(gen, stmt->as.if_stmt.condition);
            size_t jump_if_false_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);

            codegen_stmt_list(gen, &stmt->as.if_stmt.then_branch);

            size_t jump_end_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0);

            size_t else_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_if_false_addr + 1, (uint32_t)else_addr);

            if (stmt->as.if_stmt.has_else) {
                codegen_stmt_list(gen, &stmt->as.if_stmt.else_branch);
            }

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_end_addr + 1, (uint32_t)end_addr);
            break;
        }

        case STMT_LOOP_TILL: {
            size_t loop_start = gen->chunk->code_len;
            codegen_expr(gen, stmt->as.loop_till.condition);
            chunk_write_op(gen->chunk, OP_NOT);

            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);

            LoopContext loop_ctx;
            memset(&loop_ctx, 0, sizeof(loop_ctx));
            loop_ctx.enclosing = gen->current_loop;
            loop_ctx.continue_target = loop_start;
            loop_ctx.has_continue_target = 1;
            gen->current_loop = &loop_ctx;

            codegen_stmt_list(gen, &stmt->as.loop_till.body);

            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);
            for (int b = 0; b < loop_ctx.break_count; b++) {
                chunk_patch_operand(gen->chunk, loop_ctx.break_jumps[b] + 1, (uint32_t)end_addr);
            }
            free(loop_ctx.break_jumps);
            free(loop_ctx.continue_jumps);
            gen->current_loop = loop_ctx.enclosing;
            break;
        }

        case STMT_LOOP_WHILE: {
            size_t loop_start = gen->chunk->code_len;
            codegen_expr(gen, stmt->as.loop_while.condition);

            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);

            LoopContext loop_ctx;
            memset(&loop_ctx, 0, sizeof(loop_ctx));
            loop_ctx.enclosing = gen->current_loop;
            loop_ctx.continue_target = loop_start;
            loop_ctx.has_continue_target = 1;
            gen->current_loop = &loop_ctx;

            codegen_stmt_list(gen, &stmt->as.loop_while.body);

            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);
            for (int b = 0; b < loop_ctx.break_count; b++) {
                chunk_patch_operand(gen->chunk, loop_ctx.break_jumps[b] + 1, (uint32_t)end_addr);
            }
            free(loop_ctx.break_jumps);
            free(loop_ctx.continue_jumps);
            gen->current_loop = loop_ctx.enclosing;
            break;
        }

        case STMT_LOOP_RANGE: {
            codegen_expr(gen, stmt->as.loop_range.from);
            int loop_var = local_vars_add(&gen->locals, stmt->as.loop_range.var_name);
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)loop_var);

            codegen_expr(gen, stmt->as.loop_range.to);
            int limit_var = local_vars_add(&gen->locals, "__limit_temp");
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)limit_var);

            size_t loop_start = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)loop_var);
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)limit_var);
            chunk_write_op(gen->chunk, OP_LESS_THAN_EQUAL);

            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);

            LoopContext loop_ctx;
            memset(&loop_ctx, 0, sizeof(loop_ctx));
            loop_ctx.enclosing = gen->current_loop;
            gen->current_loop = &loop_ctx;

            codegen_stmt_list(gen, &stmt->as.loop_range.body);

            size_t cont_target = gen->chunk->code_len;
            loop_context_patch_continues(&loop_ctx, gen->chunk, cont_target);

            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)loop_var);
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_number(gen->chunk, 1.0));
            chunk_write_op(gen->chunk, OP_ADD);
            chunk_write_op(gen->chunk, OP_SET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)loop_var);

            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);
            for (int b = 0; b < loop_ctx.break_count; b++) {
                chunk_patch_operand(gen->chunk, loop_ctx.break_jumps[b] + 1, (uint32_t)end_addr);
            }
            free(loop_ctx.break_jumps);
            free(loop_ctx.continue_jumps);
            gen->current_loop = loop_ctx.enclosing;

            local_vars_pop(gen);
            local_vars_pop(gen);
            break;
        }

        case STMT_LOOP_THROUGH: {
            codegen_expr(gen, stmt->as.loop_through.collection);

            int array_var = local_vars_add(&gen->locals, "__array_temp");
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)array_var);

            int counter_var = local_vars_add(&gen->locals, "__counter_temp");
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_number(gen->chunk, 0.0));
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)counter_var);

            int item_var = local_vars_add(&gen->locals, stmt->as.loop_through.item_name);
            chunk_write_op(gen->chunk, OP_PUSH_NONE);
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)item_var);

            size_t loop_start = gen->chunk->code_len;

            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)counter_var);
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)array_var);
            chunk_write_op(gen->chunk, OP_LENGTH);
            chunk_write_op(gen->chunk, OP_LESS_THAN);

            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);

            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)array_var);
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)counter_var);
            chunk_write_op(gen->chunk, OP_INDEX);
            chunk_write_op(gen->chunk, OP_SET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)item_var);

            LoopContext loop_ctx;
            memset(&loop_ctx, 0, sizeof(loop_ctx));
            loop_ctx.enclosing = gen->current_loop;
            gen->current_loop = &loop_ctx;

            codegen_stmt_list(gen, &stmt->as.loop_through.body);

            size_t cont_target = gen->chunk->code_len;
            loop_context_patch_continues(&loop_ctx, gen->chunk, cont_target);

            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)counter_var);
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, (uint32_t)chunk_add_number(gen->chunk, 1.0));
            chunk_write_op(gen->chunk, OP_ADD);
            chunk_write_op(gen->chunk, OP_SET_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)counter_var);

            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);
            for (int b = 0; b < loop_ctx.break_count; b++) {
                chunk_patch_operand(gen->chunk, loop_ctx.break_jumps[b] + 1, (uint32_t)end_addr);
            }
            free(loop_ctx.break_jumps);
            free(loop_ctx.continue_jumps);
            gen->current_loop = loop_ctx.enclosing;

            local_vars_pop(gen);
            local_vars_pop(gen);
            local_vars_pop(gen);
            break;
        }

        case STMT_BREAK: {
            if (!gen->current_loop) {
                fprintf(stderr, "Compilation error: 'break' outside of loop\n");
                return;
            }
            size_t break_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0);
            loop_context_add_break(gen->current_loop, break_addr);
            break;
        }

        case STMT_CONTINUE: {
            if (!gen->current_loop) {
                fprintf(stderr, "Compilation error: 'continue' outside of loop\n");
                return;
            }
            if (gen->current_loop->has_continue_target) {
                chunk_write_op(gen->chunk, OP_JUMP);
                chunk_write_operand(gen->chunk, (uint32_t)gen->current_loop->continue_target);
            } else {
                size_t cont_addr = gen->chunk->code_len;
                chunk_write_op(gen->chunk, OP_JUMP);
                chunk_write_operand(gen->chunk, 0);
                loop_context_add_continue(gen->current_loop, cont_addr);
            }
            break;
        }

        case STMT_FUNC_DECL: {
            size_t skip_jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0);

            uint32_t func_offset = (uint32_t)gen->chunk->code_len;
            int func_idx = chunk_find_function(gen->chunk, stmt->as.func_decl.name);
            if (func_idx >= 0) {
                gen->chunk->functions[func_idx].code_offset = func_offset;
                gen->chunk->functions[func_idx].param_count = stmt->as.func_decl.params.count;
                if (stmt->file) {
                    free(gen->chunk->functions[func_idx].file);
                    gen->chunk->functions[func_idx].file = malloc(strlen(stmt->file) + 1);
                    strcpy(gen->chunk->functions[func_idx].file, stmt->file);
                }
                gen->chunk->functions[func_idx].line = stmt->line;
                gen->chunk->functions[func_idx].column = stmt->column;
            } else {
                func_idx = chunk_add_function_loc(gen->chunk, stmt->as.func_decl.name,
                                                  func_offset, stmt->as.func_decl.params.count,
                                                  stmt->file, stmt->line, stmt->column);
            }

            if (func_idx >= 0) {
                LocalVarList saved_locals = gen->locals;
                int saved_in_func = gen->in_function;
                local_vars_init(&gen->locals);
                gen->in_function = 1;

                for (int i = 0; i < stmt->as.func_decl.params.count; i++) {
                    local_vars_add(&gen->locals, stmt->as.func_decl.params.items[i]);
                }

                codegen_stmt_list(gen, &stmt->as.func_decl.body);

                chunk_write_op(gen->chunk, OP_PUSH_NONE);
                chunk_write_op(gen->chunk, OP_RETURN);

                local_vars_free(&gen->locals);
                gen->locals = saved_locals;
                gen->in_function = saved_in_func;
            }

            size_t after_func = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, skip_jump_addr + 1, (uint32_t)after_func);
            break;
        }

        case STMT_RETURN:
            if (stmt->as.return_stmt.value) {
                codegen_expr(gen, stmt->as.return_stmt.value);
            } else {
                chunk_write_op(gen->chunk, OP_PUSH_NONE);
            }
            chunk_write_op(gen->chunk, OP_RETURN);
            break;

        case STMT_OUTPUT:
            codegen_expr(gen, stmt->as.output_stmt.value);
            chunk_write_op(gen->chunk, OP_OUTPUT);
            break;

        case STMT_TRY_CATCH: {
            int locals_before_try = gen->locals.count;

            size_t try_begin_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_TRY_BEGIN);
            chunk_write_operand(gen->chunk, 0);

            codegen_stmt_list(gen, &stmt->as.try_catch.try_body);

            chunk_write_op(gen->chunk, OP_TRY_END);

            while (gen->locals.count > locals_before_try) {
                local_vars_pop(gen);
            }

            size_t jump_over_catch_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0);

            size_t catch_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, try_begin_addr + 1, (uint32_t)catch_addr);

            int err_idx = local_vars_add(&gen->locals, stmt->as.try_catch.error_name);
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, (uint32_t)err_idx);

            codegen_stmt_list(gen, &stmt->as.try_catch.catch_body);

            local_vars_pop(gen);

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_over_catch_addr + 1, (uint32_t)end_addr);
            break;
        }

        case STMT_EXPR:
            codegen_expr(gen, stmt->as.expr_stmt.expr);
            chunk_write_op(gen->chunk, OP_POP);
            break;
    }
}

CodeGen *codegen_new(void) {
    CodeGen *gen = calloc(1, sizeof(CodeGen));
    if (!gen) return NULL;

    gen->chunk = chunk_new();
    if (!gen->chunk) {
        free(gen);
        return NULL;
    }

    local_vars_init(&gen->locals);
    gen->in_function = 0;
    return gen;
}

void codegen_free(CodeGen *gen) {
    if (!gen) return;

    if (gen->chunk) {
        chunk_free(gen->chunk);
    }

    local_vars_free(&gen->locals);
    free(gen->struct_decls);
    for (int i = 0; i < gen->imported_ast_count; i++) {
        free_stmt_list(&gen->imported_asts[i]);
    }
    free(gen->imported_asts);
    free(gen);
}

static void codegen_prepass_imports_and_funcs(CodeGen *gen, Stmt **statements, int count, int is_imported_scope) {
    for (int i = 0; i < count; i++) {
        Stmt *s = statements[i];
        if (s->type == STMT_FUNC_DECL) {
            if (is_imported_scope && strcmp(s->as.func_decl.name, "main") == 0) {
                continue;
            }
            int existing = chunk_find_function(gen->chunk, s->as.func_decl.name);
            if (existing < 0) {
                chunk_add_function_loc(gen->chunk, s->as.func_decl.name, 0,
                                       s->as.func_decl.params.count,
                                       s->file, s->line, s->column);
            }
        } else if (s->type == STMT_STRUCT_DECL) {
            register_struct_in_chunk(gen, s);
        } else if (s->type == STMT_IMPORT) {
            char *resolved = module_resolve_path(s->as.import_stmt.path, module_get_current_file());
            if (!resolved) continue;
            if (module_is_imported(resolved)) {
                free(resolved);
                continue;
            }
            module_mark_imported(resolved);
            StmtList mod_stmts;
            char *err = NULL;
            char prev_file[4096];
            const char *cur_file = module_get_current_file();
            strncpy(prev_file, cur_file ? cur_file : "", sizeof(prev_file) - 1);
            prev_file[sizeof(prev_file) - 1] = '\0';
            module_set_current_file(resolved);
            if (module_parse_file(resolved, &mod_stmts, &err)) {
                codegen_retain_ast(gen, mod_stmts);
                codegen_prepass_imports_and_funcs(gen, mod_stmts.items, mod_stmts.count, 1);
            } else {
                if (err) free(err);
            }
            module_set_current_file(prev_file);
            free(resolved);
        }
    }
}

Chunk *codegen_compile(CodeGen *gen, Stmt **statements, int count) {
    local_vars_clear(&gen->locals);
    gen->in_function = 0;
    gen->current_loop = NULL;

    module_system_init();
    codegen_prepass_imports_and_funcs(gen, statements, count, 0);

    module_system_init();

    for (int i = 0; i < count; i++) {
        codegen_stmt(gen, statements[i]);
    }

    int main_idx = chunk_find_function(gen->chunk, "main");
    if (main_idx >= 0) {
        FunctionDef *mfn = &gen->chunk->functions[main_idx];
        chunk_set_location(gen->chunk, mfn->file, mfn->line, mfn->column);
        chunk_write_op(gen->chunk, OP_CALL);
        chunk_write_operand(gen->chunk, (uint32_t)main_idx);
        chunk_write_op(gen->chunk, OP_POP);
    }

    chunk_write_op(gen->chunk, OP_HALT);
    return gen->chunk;
}