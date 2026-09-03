#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/codegen.h"

typedef struct {
    char **names;
    int count;
    int capacity;
} LocalVarList;

typedef struct {
    char *name;
    int addr;
} FuncInfo;

typedef struct {
    FuncInfo *funcs;
    int count;
    int capacity;
} FuncList;

struct CodeGen {
    Chunk *chunk;
    /* Local variables are tracked per-function, frame-relative (0-based).
     * At runtime OP_GET_LOCAL/OP_SET_LOCAL/OP_DEFINE_LOCAL operands are
     * these frame-relative indices; the VM adds the current call frame's
     * local_var_base to get the real stack slot. Top-level code runs in
     * an implicit frame with base 0, so frame-relative == absolute there. */
    LocalVarList locals;
    FuncList functions;
};

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
        vars->names = realloc(vars->names, vars->capacity * sizeof(char *));
    }
    vars->names[vars->count] = malloc(strlen(name) + 1);
    strcpy(vars->names[vars->count], name);
    return vars->count++;
}

static int local_vars_find(LocalVarList *vars, const char *name) {
    /* Search from the end so inner/newer shadowing declarations win. */
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

/* Drop the most recently added local (used when a temporary local, such as
 * a loop counter, goes out of scope). Also emits the OP_POP needed to keep
 * the actual VM stack in sync with these frame-relative indices. */
static void local_vars_pop(CodeGen *gen) {
    LocalVarList *vars = &gen->locals;
    if (vars->count == 0) return;
    vars->count--;
    free(vars->names[vars->count]);
    chunk_write_op(gen->chunk, OP_POP);
}

/* Forward declarations */
static void codegen_stmt(CodeGen *gen, Stmt *stmt);
static void codegen_expr(CodeGen *gen, Expr *expr);

static void codegen_stmt_list(CodeGen *gen, StmtList *stmts) {
    for (int i = 0; i < stmts->count; i++) {
        codegen_stmt(gen, stmts->items[i]);
    }
}

static void codegen_expr(CodeGen *gen, Expr *expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_NUMBER:
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, chunk_add_number(gen->chunk, expr->as.number_val));
            break;

        case EXPR_STRING: {
            chunk_write_op(gen->chunk, OP_PUSH_STRING);
            chunk_write_operand(gen->chunk, chunk_add_string(gen->chunk, expr->as.string_val));
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
                chunk_write_operand(gen->chunk, idx);
            } else {
                chunk_write_op(gen->chunk, OP_GET_GLOBAL);
                chunk_write_operand(gen->chunk, chunk_add_string(gen->chunk, expr->as.ident_name));
            }
            break;
        }

        case EXPR_ARRAY: {
            for (int i = 0; i < expr->as.array.elements.count; i++) {
                codegen_expr(gen, expr->as.array.elements.items[i]);
            }
            chunk_write_op(gen->chunk, OP_PUSH_ARRAY);
            chunk_write_operand(gen->chunk, expr->as.array.elements.count);
            break;
        }

        case EXPR_INDEX:
            codegen_expr(gen, expr->as.index_expr.collection);
            codegen_expr(gen, expr->as.index_expr.index);
            chunk_write_op(gen->chunk, OP_INDEX);
            break;

        case EXPR_BINARY: {
            codegen_expr(gen, expr->as.binary.left);
            codegen_expr(gen, expr->as.binary.right);

            switch (expr->as.binary.op) {
                case BIN_ADD: chunk_write_op(gen->chunk, OP_ADD); break;
                case BIN_SUB: chunk_write_op(gen->chunk, OP_SUBTRACT); break;
                case BIN_MUL: chunk_write_op(gen->chunk, OP_MULTIPLY); break;
                case BIN_DIV: chunk_write_op(gen->chunk, OP_DIVIDE); break;
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
            for (int i = 0; i < expr->as.call.args.count; i++) {
                codegen_expr(gen, expr->as.call.args.items[i]);
            }

            /* Check for builtins */
            const char *name = expr->as.call.callee_name;
            if (strcmp(name, "output") == 0) {
                /* Not normally reached (output is its own statement type),
                 * but keep the CALL contract of leaving one value behind. */
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
            } else {
                /* User-defined function */
                int func_idx = chunk_find_function(gen->chunk, name);
                if (func_idx >= 0) {
                    chunk_write_op(gen->chunk, OP_CALL);
                    chunk_write_operand(gen->chunk, func_idx);
                } else {
                    /* Unknown function: keep the stack balanced */
                    chunk_write_op(gen->chunk, OP_PUSH_NONE);
                }
            }
            break;
        }
    }
}

static void codegen_stmt(CodeGen *gen, Stmt *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_VAR_DECL: {
            codegen_expr(gen, stmt->as.var_decl.value);
            if (stmt->as.var_decl.is_global) {
                chunk_write_op(gen->chunk, OP_SET_GLOBAL);
                chunk_write_operand(gen->chunk, chunk_add_string(gen->chunk, stmt->as.var_decl.name));
                chunk_write_op(gen->chunk, OP_POP);
            } else {
                int idx = local_vars_add(&gen->locals, stmt->as.var_decl.name);
                chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
                chunk_write_operand(gen->chunk, idx);
            }
            break;
        }

        case STMT_ASSIGN: {
            codegen_expr(gen, stmt->as.assign.value);
            int idx = local_vars_find(&gen->locals, stmt->as.assign.name);
            if (idx >= 0) {
                chunk_write_op(gen->chunk, OP_SET_LOCAL);
                chunk_write_operand(gen->chunk, idx);
            } else {
                chunk_write_op(gen->chunk, OP_SET_GLOBAL);
                chunk_write_operand(gen->chunk, chunk_add_string(gen->chunk, stmt->as.assign.name));
                chunk_write_op(gen->chunk, OP_POP);
            }
            break;
        }

        case STMT_IF: {
            codegen_expr(gen, stmt->as.if_stmt.condition);
            size_t jump_if_false_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0); /* Placeholder */

            codegen_stmt_list(gen, &stmt->as.if_stmt.then_branch);

            size_t jump_end_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0); /* Placeholder */

            /* Patch jump_if_false to land here, right before the else branch */
            size_t else_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_if_false_addr + 1, (uint32_t)else_addr);

            if (stmt->as.if_stmt.has_else) {
                codegen_stmt_list(gen, &stmt->as.if_stmt.else_branch);
            }

            /* Patch jump_end to land after the whole if/else */
            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_end_addr + 1, (uint32_t)end_addr);
            break;
        }

        case STMT_LOOP_TILL: {
            /* "loop till <cond>" runs the body repeatedly while <cond> is
             * false, and stops as soon as <cond> becomes true (matching the
             * tree-walking interpreter). OP_JUMP_IF_FALSE only branches
             * when a value is false, so negate the condition first to turn
             * "exit when true" into "exit when NOT(cond) is false". */
            size_t loop_start = gen->chunk->code_len;
            codegen_expr(gen, stmt->as.loop_till.condition);
            chunk_write_op(gen->chunk, OP_NOT);

            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0); /* Placeholder */

            codegen_stmt_list(gen, &stmt->as.loop_till.body);

            /* Jump back to start */
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            /* Patch exit jump */
            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);
            break;
        }

        case STMT_LOOP_RANGE: {
            codegen_expr(gen, stmt->as.loop_range.from);
            int loop_var = local_vars_add(&gen->locals, stmt->as.loop_range.var_name);
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, loop_var);

            size_t loop_start = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, loop_var);
            codegen_expr(gen, stmt->as.loop_range.to);
            /* "loop from X to Y" is inclusive of Y (matches the tree-walking
             * interpreter and the language documentation). */
            chunk_write_op(gen->chunk, OP_LESS_THAN_EQUAL);

            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);

            codegen_stmt_list(gen, &stmt->as.loop_range.body);

            /* Increment loop variable */
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, loop_var);
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, chunk_add_number(gen->chunk, 1.0));
            chunk_write_op(gen->chunk, OP_ADD);
            chunk_write_op(gen->chunk, OP_SET_LOCAL);
            chunk_write_operand(gen->chunk, loop_var);

            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);

            /* Drop the loop variable and emit the matching OP_POP so the
             * real VM stack stays aligned with our frame-relative indices. */
            local_vars_pop(gen);
            break;
        }

        case STMT_LOOP_THROUGH: {
            /* Evaluate collection and push on stack */
            codegen_expr(gen, stmt->as.loop_through.collection);

            /* Store collection in a temporary local variable */
            int array_var = local_vars_add(&gen->locals, "__array_temp");
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, array_var);

            /* Initialize counter to 0 */
            int counter_var = local_vars_add(&gen->locals, "__counter_temp");
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, chunk_add_number(gen->chunk, 0.0));
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, counter_var);

            /* Add item variable */
            int item_var = local_vars_add(&gen->locals, stmt->as.loop_through.item_name);
            chunk_write_op(gen->chunk, OP_PUSH_NONE);
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, item_var);

            /* Loop start */
            size_t loop_start = gen->chunk->code_len;

            /* Get counter < array.length */
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, counter_var);
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, array_var);
            chunk_write_op(gen->chunk, OP_LENGTH);
            chunk_write_op(gen->chunk, OP_LESS_THAN);

            /* Jump if false */
            size_t jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP_IF_FALSE);
            chunk_write_operand(gen->chunk, 0);  /* Placeholder */

            /* Get array[counter] and store in item variable */
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, array_var);
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, counter_var);
            chunk_write_op(gen->chunk, OP_INDEX);
            chunk_write_op(gen->chunk, OP_SET_LOCAL);
            chunk_write_operand(gen->chunk, item_var);

            /* Execute loop body */
            codegen_stmt_list(gen, &stmt->as.loop_through.body);

            /* Increment counter */
            chunk_write_op(gen->chunk, OP_GET_LOCAL);
            chunk_write_operand(gen->chunk, counter_var);
            chunk_write_op(gen->chunk, OP_PUSH_NUMBER);
            chunk_write_operand(gen->chunk, chunk_add_number(gen->chunk, 1.0));
            chunk_write_op(gen->chunk, OP_ADD);
            chunk_write_op(gen->chunk, OP_SET_LOCAL);
            chunk_write_operand(gen->chunk, counter_var);

            /* Jump back to start */
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, (uint32_t)loop_start);

            /* Patch jump address */
            size_t end_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, jump_addr + 1, (uint32_t)end_addr);

            /* Clean up temporary variables: item, counter, array (LIFO) */
            local_vars_pop(gen);
            local_vars_pop(gen);
            local_vars_pop(gen);
            break;
        }

        case STMT_FUNC_DECL: {
            /* Function bodies are compiled inline but skipped over at
             * runtime via a forward jump, since OP_CALL jumps directly to
             * the recorded code_offset instead of falling through. */
            size_t skip_jump_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0); /* Placeholder */

            uint32_t func_offset = gen->chunk->code_len;
            int func_idx = chunk_add_function(gen->chunk, stmt->as.func_decl.name,
                                              func_offset, stmt->as.func_decl.params.count);

            if (func_idx >= 0) {
                /* Save and clear local variables */
                LocalVarList saved_locals = gen->locals;
                local_vars_init(&gen->locals);

                /* Add parameters as local variables (already on the stack
                 * at call time, at frame-relative slots 0..param_count-1) */
                for (int i = 0; i < stmt->as.func_decl.params.count; i++) {
                    local_vars_add(&gen->locals, stmt->as.func_decl.params.items[i]);
                }

                /* Compile function body */
                codegen_stmt_list(gen, &stmt->as.func_decl.body);

                /* Implicit return none if no explicit return */
                chunk_write_op(gen->chunk, OP_PUSH_NONE);
                chunk_write_op(gen->chunk, OP_RETURN);

                /* Restore previous local variables */
                local_vars_free(&gen->locals);
                gen->locals = saved_locals;
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
            /* OP_TRY_BEGIN pushes a handler that, if a runtime error is
             * raised anywhere before the matching OP_TRY_END, unwinds the
             * stack and jumps to the catch target with the error message
             * pushed as a string. The VM truncates its stack back to the
             * sp at OP_TRY_BEGIN time, so any locals declared inside the
             * try body no longer exist when the catch block runs - we must
             * mirror that here by rolling the codegen's own locals list
             * back to the same point before assigning the catch variable's
             * slot, or the two will disagree about where things live. */
            int locals_before_try = gen->locals.count;

            size_t try_begin_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_TRY_BEGIN);
            chunk_write_operand(gen->chunk, 0); /* Placeholder: catch target */

            codegen_stmt_list(gen, &stmt->as.try_catch.try_body);

            chunk_write_op(gen->chunk, OP_TRY_END);

            /* Drop any locals the try body declared (their OP_POPs already
             * happened implicitly via the VM's stack truncation on error,
             * and via normal fall-through we still need to balance them -
             * but on the success path nothing raised, so pop them here). */
            while (gen->locals.count > locals_before_try) {
                local_vars_pop(gen);
            }

            size_t jump_over_catch_addr = gen->chunk->code_len;
            chunk_write_op(gen->chunk, OP_JUMP);
            chunk_write_operand(gen->chunk, 0); /* Placeholder */

            size_t catch_addr = gen->chunk->code_len;
            chunk_patch_operand(gen->chunk, try_begin_addr + 1, (uint32_t)catch_addr);

            /* On the error path the VM has already truncated its stack back
             * to try-begin time and pushed the error message, so the
             * codegen's locals list must also be back at locals_before_try
             * here - which it is, since we rolled it back above before
             * emitting the jump-over-catch. */
            int err_idx = local_vars_add(&gen->locals, stmt->as.try_catch.error_name);
            chunk_write_op(gen->chunk, OP_DEFINE_LOCAL);
            chunk_write_operand(gen->chunk, err_idx);

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
    CodeGen *gen = malloc(sizeof(CodeGen));
    if (!gen) return NULL;

    gen->chunk = chunk_new();
    if (!gen->chunk) {
        free(gen);
        return NULL;
    }

    local_vars_init(&gen->locals);

    gen->functions.funcs = malloc(32 * sizeof(FuncInfo));
    gen->functions.capacity = 32;
    gen->functions.count = 0;

    return gen;
}

void codegen_free(CodeGen *gen) {
    if (!gen) return;

    if (gen->chunk) {
        chunk_free(gen->chunk);
    }

    local_vars_free(&gen->locals);

    for (int i = 0; i < gen->functions.count; i++) {
        free(gen->functions.funcs[i].name);
    }
    free(gen->functions.funcs);

    free(gen);
}

Chunk *codegen_compile(CodeGen *gen, Stmt **statements, int count) {
    local_vars_clear(&gen->locals);

    /* Compile all statements (including function declarations, which emit
     * their bodies out-of-line and jump around them at top level) */
    for (int i = 0; i < count; i++) {
        codegen_stmt(gen, statements[i]);
    }

    /* Find and call main if it exists */
    int main_idx = chunk_find_function(gen->chunk, "main");
    if (main_idx >= 0) {
        chunk_write_op(gen->chunk, OP_CALL);
        chunk_write_operand(gen->chunk, main_idx);
        chunk_write_op(gen->chunk, OP_POP);  /* Discard return value */
    }

    chunk_write_op(gen->chunk, OP_HALT);
    return gen->chunk;
}