#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../include/vm.h"
#include "../include/value.h"
#include "../include/builtins.h"

static ObjString *chunk_get_string_obj(Chunk *chunk, uint32_t idx) {
    if (idx >= chunk->strings_len) return intern_string("", 0);
    if (!chunk->string_objs[idx]) {
        chunk->string_objs[idx] = intern_string(chunk->strings[idx], strlen(chunk->strings[idx]));
    }
    return chunk->string_objs[idx];
}

VM *vm_new(Chunk *chunk) {
    VM *vm = malloc(sizeof(VM));
    if (!vm) return NULL;
    memset(vm, 0, sizeof(VM));

    vm->chunk = chunk;
    vm->ip = chunk ? chunk->code : NULL;

    vm->stack_cap = VM_INITIAL_STACK_SIZE;
    vm->stack = malloc((size_t)vm->stack_cap * sizeof(Value));
    vm->sp = 0;

    vm->call_cap = VM_INITIAL_CALL_SIZE;
    vm->call_stack = malloc((size_t)vm->call_cap * sizeof(CallFrame));
    vm->call_sp = 0;

    int init_globals = 64;
    if (chunk && (int)chunk->globals_len >= init_globals) {
        init_globals = (int)chunk->globals_len + 16;
    }
    vm->global_cap = init_globals;
    vm->global_count = chunk ? (int)chunk->globals_len : 0;
    vm->globals = malloc((size_t)vm->global_cap * sizeof(Value));
    vm->global_defined = calloc((size_t)vm->global_cap, sizeof(uint8_t));
    if (vm->globals) {
        for (int i = 0; i < vm->global_cap; i++) {
            vm->globals[i] = make_none_value();
        }
    }

    vm->try_cap = VM_INITIAL_TRY_SIZE;
    vm->try_stack = malloc((size_t)vm->try_cap * sizeof(TryFrame));
    vm->try_sp = 0;

    if (!vm->stack || !vm->call_stack || !vm->globals || !vm->global_defined || !vm->try_stack) {
        free(vm->stack);
        free(vm->call_stack);
        free(vm->globals);
        free(vm->global_defined);
        free(vm->try_stack);
        free(vm);
        return NULL;
    }

    gc_register_vm(vm);
    return vm;
}

void vm_free(VM *vm) {
    if (!vm) return;
    gc_unregister_vm(vm);
    if (vm->has_saved_main) {
        free(vm->main_ctx.stack);
        free(vm->main_ctx.call_stack);
        free(vm->main_ctx.try_stack);
    } else {
        free(vm->stack);
        free(vm->call_stack);
        free(vm->try_stack);
    }
    free(vm->globals);
    free(vm->global_defined);
    free(vm->pinned_coros);
    free(vm);
}

void vm_pin_coroutine(VM *vm, ObjCoroutine *co) {
    if (!vm || !co) return;
    for (int i = 0; i < vm->pinned_coro_count; i++) {
        if (vm->pinned_coros[i] == co) return;
    }
    if (vm->pinned_coro_count >= vm->pinned_coro_cap) {
        int new_cap = vm->pinned_coro_cap == 0 ? 8 : vm->pinned_coro_cap * 2;
        ObjCoroutine **nc = realloc(vm->pinned_coros, (size_t)new_cap * sizeof(ObjCoroutine *));
        if (!nc) return;
        vm->pinned_coros = nc;
        vm->pinned_coro_cap = new_cap;
    }
    vm->pinned_coros[vm->pinned_coro_count++] = co;
}

static void vm_ensure_global_slot(VM *vm, uint32_t slot) {
    if ((int)slot >= vm->global_cap) {
        int old_cap = vm->global_cap;
        while ((int)slot >= vm->global_cap) {
            vm->global_cap *= 2;
        }
        vm->globals = realloc(vm->globals, (size_t)vm->global_cap * sizeof(Value));
        vm->global_defined = realloc(vm->global_defined, (size_t)vm->global_cap * sizeof(uint8_t));
        for (int i = old_cap; i < vm->global_cap; i++) {
            vm->globals[i] = make_none_value();
            vm->global_defined[i] = 0;
        }
    }
    if ((int)slot >= vm->global_count) {
        vm->global_count = (int)slot + 1;
    }
}

void vm_set_global_slot(VM *vm, uint32_t slot, Value val) {
    vm_ensure_global_slot(vm, slot);
    vm->globals[slot] = val;
    vm->global_defined[slot] = 1;
}

Value vm_get_global_slot(VM *vm, uint32_t slot, int *is_defined) {
    if ((int)slot >= vm->global_count || !vm->global_defined[slot]) {
        if (is_defined) *is_defined = 0;
        return make_none_value();
    }
    if (is_defined) *is_defined = 1;
    return vm->globals[slot];
}

void vm_set_global_by_name(VM *vm, const char *name, Value val) {
    if (!vm || !vm->chunk || !name) return;
    uint32_t slot = chunk_resolve_global(vm->chunk, name);
    vm_set_global_slot(vm, slot, val);
}

int vm_get_global_by_name(VM *vm, const char *name, Value *out) {
    if (!vm || !vm->chunk || !name) return 0;
    int slot = chunk_find_global(vm->chunk, name);
    if (slot < 0 || slot >= vm->global_count || !vm->global_defined[slot]) {
        if (out) *out = make_none_value();
        return 0;
    }
    if (out) *out = vm->globals[slot];
    return 1;
}

void vm_push(VM *vm, Value val) {
    if (vm->sp >= vm->stack_cap) {
        if (vm->stack_cap >= VM_MAX_STACK_SIZE) {
            fprintf(stderr, "Runtime error: stack overflow\n");
            vm->had_error = 1;
            return;
        }
        int new_cap = vm->stack_cap * 2;
        if (new_cap > VM_MAX_STACK_SIZE) new_cap = VM_MAX_STACK_SIZE;
        Value *new_stack = realloc(vm->stack, (size_t)new_cap * sizeof(Value));
        if (!new_stack) {
            fprintf(stderr, "Runtime error: out of memory growing stack\n");
            vm->had_error = 1;
            return;
        }
        vm->stack = new_stack;
        vm->stack_cap = new_cap;
        if (vm->current_coro) {
            vm->current_coro->stack = vm->stack;
            vm->current_coro->stack_cap = vm->stack_cap;
        }
    }
    vm->stack[vm->sp++] = val;
    if (vm->current_coro) {
        vm->current_coro->sp = vm->sp;
    }
}

Value vm_pop(VM *vm) {
    if (vm->sp <= 0) {
        fprintf(stderr, "Runtime error: stack underflow\n");
        vm->had_error = 1;
        return make_none_value();
    }
    Value v = vm->stack[--vm->sp];
    if (vm->current_coro) {
        vm->current_coro->sp = vm->sp;
    }
    gc_push_root(v);
    return v;
}

Value vm_peek(VM *vm) {
    if (vm->sp > 0) {
        return vm->stack[vm->sp - 1];
    }
    return make_none_value();
}

static int vm_push_call_frame(VM *vm, uint8_t *return_ip, uint8_t *call_site_ip, int func_idx, int local_var_base) {
    if (vm->call_sp >= vm->call_cap) {
        if (vm->call_cap >= VM_MAX_CALL_SIZE) {
            return 0;
        }
        int new_cap = vm->call_cap * 2;
        if (new_cap > VM_MAX_CALL_SIZE) new_cap = VM_MAX_CALL_SIZE;
        CallFrame *new_calls = realloc(vm->call_stack, (size_t)new_cap * sizeof(CallFrame));
        if (!new_calls) return 0;
        vm->call_stack = new_calls;
        vm->call_cap = new_cap;
        if (vm->current_coro) {
            vm->current_coro->call_stack = vm->call_stack;
            vm->current_coro->call_cap = vm->call_cap;
        }
    }
    vm->call_stack[vm->call_sp].return_ip = return_ip;
    vm->call_stack[vm->call_sp].call_site_ip = call_site_ip;
    vm->call_stack[vm->call_sp].func_idx = func_idx;
    vm->call_stack[vm->call_sp].local_var_base = local_var_base;
    vm->call_stack[vm->call_sp].try_sp = vm->try_sp;
    vm->call_sp++;
    if (vm->current_coro) {
        vm->current_coro->call_sp = vm->call_sp;
    }
    return 1;
}

static int vm_push_try_frame(VM *vm, uint8_t *catch_ip) {
    if (vm->try_sp >= vm->try_cap) {
        if (vm->try_cap >= VM_MAX_TRY_SIZE) return 0;
        int new_cap = vm->try_cap * 2;
        if (new_cap > VM_MAX_TRY_SIZE) new_cap = VM_MAX_TRY_SIZE;
        TryFrame *new_tries = realloc(vm->try_stack, (size_t)new_cap * sizeof(TryFrame));
        if (!new_tries) return 0;
        vm->try_stack = new_tries;
        vm->try_cap = new_cap;
        if (vm->current_coro) {
            vm->current_coro->try_stack = vm->try_stack;
            vm->current_coro->try_cap = vm->try_cap;
        }
    }
    vm->try_stack[vm->try_sp].catch_ip = catch_ip;
    vm->try_stack[vm->try_sp].sp = vm->sp;
    vm->try_stack[vm->try_sp].call_sp = vm->call_sp;
    vm->try_sp++;
    if (vm->current_coro) {
        vm->current_coro->try_sp = vm->try_sp;
    }
    return 1;
}

static void vm_save_active_to_coro(VM *vm, ObjCoroutine *co) {
    co->ip = vm->ip;
    co->stack = vm->stack;
    co->sp = vm->sp;
    co->stack_cap = vm->stack_cap;
    co->call_stack = vm->call_stack;
    co->call_sp = vm->call_sp;
    co->call_cap = vm->call_cap;
    co->try_stack = vm->try_stack;
    co->try_sp = vm->try_sp;
    co->try_cap = vm->try_cap;
}

static void vm_load_active_from_coro(VM *vm, ObjCoroutine *co) {
    vm->ip = co->ip;
    vm->stack = co->stack;
    vm->sp = co->sp;
    vm->stack_cap = co->stack_cap;
    vm->call_stack = co->call_stack;
    vm->call_sp = co->call_sp;
    vm->call_cap = co->call_cap;
    vm->try_stack = co->try_stack;
    vm->try_sp = co->try_sp;
    vm->try_cap = co->try_cap;
    vm->current_coro = co;
}

static void vm_save_active_to_main(VM *vm) {
    vm->main_ctx.ip = vm->ip;
    vm->main_ctx.stack = vm->stack;
    vm->main_ctx.sp = vm->sp;
    vm->main_ctx.stack_cap = vm->stack_cap;
    vm->main_ctx.call_stack = vm->call_stack;
    vm->main_ctx.call_sp = vm->call_sp;
    vm->main_ctx.call_cap = vm->call_cap;
    vm->main_ctx.try_stack = vm->try_stack;
    vm->main_ctx.try_sp = vm->try_sp;
    vm->main_ctx.try_cap = vm->try_cap;
    vm->has_saved_main = 1;
}

static void vm_load_active_from_main(VM *vm) {
    vm->ip = vm->main_ctx.ip;
    vm->stack = vm->main_ctx.stack;
    vm->sp = vm->main_ctx.sp;
    vm->stack_cap = vm->main_ctx.stack_cap;
    vm->call_stack = vm->main_ctx.call_stack;
    vm->call_sp = vm->main_ctx.call_sp;
    vm->call_cap = vm->main_ctx.call_cap;
    vm->try_stack = vm->main_ctx.try_stack;
    vm->try_sp = vm->main_ctx.try_sp;
    vm->try_cap = vm->main_ctx.try_cap;
    vm->current_coro = NULL;
    vm->has_saved_main = 0;
}

static uint32_t read_operand(VM *vm) {
    uint32_t value = 0;
    value |= ((uint32_t)*vm->ip++) << 24;
    value |= ((uint32_t)*vm->ip++) << 16;
    value |= ((uint32_t)*vm->ip++) << 8;
    value |= (uint32_t)*vm->ip++;
    return value;
}

static Value to_number(Value v) {
    if (v.type == VAL_NUMBER) return v;
    if (v.type == VAL_STRING) {
        char *endptr;
        double num = strtod(v.as.string, &endptr);
        if (*endptr == '\0' || (*endptr == ' ' && *(endptr+1) == '\0')) {
            return make_number_value(num);
        }
        return make_number_value(0.0);
    }
    if (v.type == VAL_BOOL) {
        return make_number_value(v.as.boolean ? 1.0 : 0.0);
    }
    return make_number_value(0.0);
}

static Value to_bool(Value v) {
    if (v.type == VAL_BOOL) return v;
    if (v.type == VAL_NUMBER) {
        return make_bool_value(v.as.number != 0.0);
    }
    if (v.type == VAL_STRING) {
        return make_bool_value(strlen(v.as.string) > 0);
    }
    if (v.type == VAL_NONE) {
        return make_bool_value(0);
    }
    return make_bool_value(1);
}

static void vm_build_stack_trace(VM *vm, const char *message, uint8_t *err_ip) {
    char *buf = vm->stack_trace;
    size_t cap = sizeof(vm->stack_trace);
    size_t pos = 0;

    const char *file = "<script>";
    int line = 0, col = 0;
    uint32_t offset = (err_ip && vm->chunk && err_ip >= vm->chunk->code)
                      ? (uint32_t)(err_ip - vm->chunk->code) : 0;
    chunk_get_location(vm->chunk, offset, &file, &line, &col);

    const char *cur_func = "<top-level>";
    if (vm->call_sp > 0) {
        int fidx = vm->call_stack[vm->call_sp - 1].func_idx;
        if (fidx >= 0 && (size_t)fidx < vm->chunk->functions_len) {
            cur_func = vm->chunk->functions[fidx].name;
        }
    }

    int written = snprintf(buf + pos, cap - pos,
                           "Runtime error: %s\n  at %s (%s:%d:%d)",
                           message, cur_func, file, line, col);
    if (written > 0 && (size_t)written < cap - pos) pos += (size_t)written;

    for (int i = vm->call_sp - 1; i >= 0; i--) {
        CallFrame *frame = &vm->call_stack[i];
        if (!frame->return_ip) continue; /* Coroutine base frame */
        uint32_t call_off = (frame->call_site_ip && frame->call_site_ip >= vm->chunk->code)
                            ? (uint32_t)(frame->call_site_ip - vm->chunk->code) : 0;
        const char *cfile = "<script>";
        int cline = 0, ccol = 0;
        chunk_get_location(vm->chunk, call_off, &cfile, &cline, &ccol);

        const char *caller_name = "<top-level>";
        if (i > 0) {
            int cfidx = vm->call_stack[i - 1].func_idx;
            if (cfidx >= 0 && (size_t)cfidx < vm->chunk->functions_len) {
                caller_name = vm->chunk->functions[cfidx].name;
            }
        }
        written = snprintf(buf + pos, cap - pos,
                           "\n  at %s (%s:%d:%d)",
                           caller_name, cfile, cline, ccol);
        if (written > 0 && (size_t)written < cap - pos) pos += (size_t)written;
    }

    /* If inside a coroutine, also walk caller coroutines and main context */
    ObjCoroutine *co = vm->current_coro ? vm->current_coro->caller : NULL;
    while (co) {
        for (int i = co->call_sp - 1; i >= 0; i--) {
            CallFrame *frame = &co->call_stack[i];
            if (!frame->return_ip) continue;
            uint32_t call_off = (frame->call_site_ip && frame->call_site_ip >= vm->chunk->code)
                                ? (uint32_t)(frame->call_site_ip - vm->chunk->code) : 0;
            const char *cfile = "<script>";
            int cline = 0, ccol = 0;
            chunk_get_location(vm->chunk, call_off, &cfile, &cline, &ccol);
            const char *caller_name = "<coroutine>";
            if (i > 0 && co->call_stack[i - 1].func_idx >= 0) {
                caller_name = vm->chunk->functions[co->call_stack[i - 1].func_idx].name;
            }
            written = snprintf(buf + pos, cap - pos,
                               "\n  at %s (%s:%d:%d)",
                               caller_name, cfile, cline, ccol);
            if (written > 0 && (size_t)written < cap - pos) pos += (size_t)written;
        }
        co = co->caller;
    }

    if (vm->has_saved_main) {
        for (int i = vm->main_ctx.call_sp - 1; i >= 0; i--) {
            CallFrame *frame = &vm->main_ctx.call_stack[i];
            if (!frame->return_ip) continue;
            uint32_t call_off = (frame->call_site_ip && frame->call_site_ip >= vm->chunk->code)
                                ? (uint32_t)(frame->call_site_ip - vm->chunk->code) : 0;
            const char *cfile = "<script>";
            int cline = 0, ccol = 0;
            chunk_get_location(vm->chunk, call_off, &cfile, &cline, &ccol);
            const char *caller_name = "<top-level>";
            if (i > 0 && vm->main_ctx.call_stack[i - 1].func_idx >= 0) {
                caller_name = vm->chunk->functions[vm->main_ctx.call_stack[i - 1].func_idx].name;
            }
            written = snprintf(buf + pos, cap - pos,
                               "\n  at %s (%s:%d:%d)",
                               caller_name, cfile, cline, ccol);
            if (written > 0 && (size_t)written < cap - pos) pos += (size_t)written;
        }
    }

    vesbo_set_last_stack_trace(vm->stack_trace);
}

static void vm_raise_at(VM *vm, uint8_t *op_ip, const char *message) {
    vm_build_stack_trace(vm, message, op_ip);

    /* Unwind coroutine frames until we find an active try handler or reach main */
    while (vm->try_sp == 0 && vm->current_coro != NULL) {
        ObjCoroutine *dead_co = vm->current_coro;
        dead_co->state = CORO_DEAD;
        vm_save_active_to_coro(vm, dead_co);
        ObjCoroutine *caller = dead_co->caller;
        dead_co->caller = NULL;
        if (caller) {
            vm_load_active_from_coro(vm, caller);
        } else if (vm->has_saved_main) {
            vm_load_active_from_main(vm);
        } else {
            vm->current_coro = NULL;
            break;
        }
    }

    if (vm->try_sp > 0) {
        TryFrame frame = vm->try_stack[--vm->try_sp];
        vm->sp = frame.sp;
        vm->call_sp = frame.call_sp;
        vm->ip = frame.catch_ip;
        if (vm->current_coro) {
            vm->current_coro->try_sp = vm->try_sp;
            vm->current_coro->sp = vm->sp;
            vm->current_coro->call_sp = vm->call_sp;
        }
        vm_push(vm, make_string_value(message));
        return;
    }

    vm->had_error = 1;
    strncpy(vm->error_message, message, sizeof(vm->error_message) - 1);
    vm->error_message[sizeof(vm->error_message) - 1] = '\0';
}

static int current_local_base(VM *vm) {
    if (vm->call_sp > 0) {
        return vm->call_stack[vm->call_sp - 1].local_var_base;
    }
    return 0;
}

static int vm_enter_coroutine(VM *vm, uint8_t *op_ip, ObjCoroutine *co, int has_send_val, Value send_val, int save_caller) {
    if (!co) {
        vm_raise_at(vm, op_ip, "cannot resume null coroutine");
        return 0;
    }
    if (co->state == CORO_DEAD) {
        vm_raise_at(vm, op_ip, "cannot resume dead coroutine");
        return 0;
    }
    if (co->state == CORO_RUNNING) {
        vm_raise_at(vm, op_ip, "cannot resume already running coroutine");
        return 0;
    }

    if (!co->started) {
        if (co->func_idx < 0 && co->func_name) {
            co->func_idx = chunk_find_function(vm->chunk, co->func_name);
        }
        if (co->func_idx < 0 || (size_t)co->func_idx >= vm->chunk->functions_len) {
            co->state = CORO_DEAD;
            char msg[256];
            snprintf(msg, sizeof(msg), "undefined coroutine function '%s'", co->func_name ? co->func_name : "<unknown>");
            vm_raise_at(vm, op_ip, msg);
            return 0;
        }
        FunctionDef *func = &vm->chunk->functions[co->func_idx];
        int provided_args = co->init_arg_count;
        int use_send_as_arg = (provided_args == 0 && has_send_val && func->param_count == 1);
        int total_args = provided_args + (use_send_as_arg ? 1 : 0);
        if (total_args != func->param_count) {
            co->state = CORO_DEAD;
            char msg[256];
            snprintf(msg, sizeof(msg), "coroutine '%s' expects %d argument(s), got %d",
                     func->name, func->param_count, total_args);
            vm_raise_at(vm, op_ip, msg);
            return 0;
        }

        if (!co->stack || co->stack_cap < total_args) {
            int new_cap = co->stack_cap > 0 ? co->stack_cap : VM_INITIAL_STACK_SIZE;
            while (new_cap < total_args) new_cap *= 2;
            co->stack = realloc(co->stack, (size_t)new_cap * sizeof(Value));
            co->stack_cap = new_cap;
        }
        co->sp = 0;

        if (!co->call_stack || co->call_cap <= 0) {
            co->call_cap = VM_INITIAL_CALL_SIZE;
            co->call_stack = malloc((size_t)co->call_cap * sizeof(CallFrame));
        }
        co->call_sp = 0;

        if (!co->try_stack || co->try_cap <= 0) {
            co->try_cap = VM_INITIAL_TRY_SIZE;
            co->try_stack = malloc((size_t)co->try_cap * sizeof(TryFrame));
        }
        co->try_sp = 0;

        for (int i = 0; i < co->init_arg_count; i++) {
            co->stack[co->sp++] = co->init_args[i];
        }
        if (use_send_as_arg) {
            co->stack[co->sp++] = send_val;
        }

        /* Base coroutine frame with return_ip == NULL */
        co->call_stack[0].return_ip = NULL;
        co->call_stack[0].call_site_ip = op_ip;
        co->call_stack[0].func_idx = co->func_idx;
        co->call_stack[0].local_var_base = 0;
        co->call_stack[0].try_sp = 0;
        co->call_sp = 1;

        co->ip = vm->chunk->code + func->code_offset;
        co->started = 1;
    }

    ObjCoroutine *prev_coro = vm->current_coro;
    if (prev_coro) {
        vm_save_active_to_coro(vm, prev_coro);
    } else if (!vm->has_saved_main) {
        vm_save_active_to_main(vm);
    }
    if (save_caller) {
        co->caller = prev_coro;
    } else {
        co->caller = NULL;
        vm->main_ctx.ip = NULL;
    }

    vm_load_active_from_coro(vm, co);
    co->state = CORO_RUNNING;
    return 1;
}

int vm_execute(VM *vm) {
    while (1) {
        if (vm->had_error) {
            if (vm->stack_trace[0] != '\0') {
                fprintf(stderr, "%s\n", vm->stack_trace);
            } else {
                fprintf(stderr, "Runtime error: %s\n", vm->error_message);
            }
            return 1;
        }

        int saved_roots = gc_save_roots();
        gc_count_instruction();

        uint8_t *op_ip = vm->ip;
        OpCode op = (OpCode)*vm->ip++;

        switch (op) {
            case OP_PUSH_NUMBER: {
                uint32_t idx = read_operand(vm);
                vm_push(vm, make_number_value(vm->chunk->numbers[idx]));
                break;
            }

            case OP_PUSH_STRING: {
                uint32_t idx = read_operand(vm);
                vm_push(vm, make_obj_string_value(chunk_get_string_obj(vm->chunk, idx)));
                break;
            }

            case OP_PUSH_TRUE:
                vm_push(vm, make_bool_value(1));
                break;

            case OP_PUSH_FALSE:
                vm_push(vm, make_bool_value(0));
                break;

            case OP_PUSH_NONE:
                vm_push(vm, make_none_value());
                break;

            case OP_PUSH_ARRAY: {
                uint32_t count = read_operand(vm);
                Value arr = make_array_value();

                if (count > 0) {
                    Value *temp = malloc(count * sizeof(Value));
                    for (uint32_t i = 0; i < count; i++) {
                        temp[i] = vm_pop(vm);
                    }
                    for (int i = (int)count - 1; i >= 0; i--) {
                        value_array_push(arr.as.array, temp[i]);
                    }
                    free(temp);
                }

                vm_push(vm, arr);
                break;
            }

            case OP_BUILD_MAP: {
                uint32_t count = read_operand(vm);
                Value map_val = make_map_value();

                if (count > 0) {
                    Value *temp = malloc(count * 2 * sizeof(Value));
                    for (int i = (int)(count * 2) - 1; i >= 0; i--) {
                        temp[i] = vm_pop(vm);
                    }
                    for (uint32_t i = 0; i < count; i++) {
                        Value key = temp[i * 2];
                        Value val = temp[i * 2 + 1];
                        if (key.type == VAL_STRING) {
                            value_map_set_obj(map_val.as.map, AS_OBJ_STRING(key), val);
                        } else {
                            char *kstr = value_to_display_string(key);
                            value_map_set(map_val.as.map, kstr, val);
                            free(kstr);
                        }
                    }
                    free(temp);
                }

                vm_push(vm, map_val);
                break;
            }

            case OP_DEFINE_LOCAL: {
                uint32_t idx = read_operand(vm);
                int slot = current_local_base(vm) + (int)idx;
                Value val = vm_pop(vm);
                if (slot == vm->sp) {
                    vm_push(vm, val);
                } else if (slot >= 0 && slot < vm->sp) {
                    vm->stack[slot] = val;
                } else {
                    while (vm->sp < slot) {
                        vm_push(vm, make_none_value());
                    }
                    vm_push(vm, val);
                }
                break;
            }

            case OP_GET_LOCAL: {
                uint32_t idx = read_operand(vm);
                int slot = current_local_base(vm) + (int)idx;
                if (slot >= 0 && slot < vm->sp) {
                    vm_push(vm, vm->stack[slot]);
                } else {
                    vm_push(vm, make_none_value());
                }
                break;
            }

            case OP_SET_LOCAL: {
                uint32_t idx = read_operand(vm);
                int slot = current_local_base(vm) + (int)idx;
                Value val = vm_pop(vm);
                if (slot >= 0 && slot < vm->sp) {
                    vm->stack[slot] = val;
                }
                break;
            }

            case OP_GET_GLOBAL: {
                uint32_t slot = read_operand(vm);
                if ((int)slot < vm->global_count && vm->global_defined[slot]) {
                    vm_push(vm, vm->globals[slot]);
                } else {
                    vm_push(vm, make_none_value());
                }
                break;
            }

            case OP_SET_GLOBAL: {
                uint32_t slot = read_operand(vm);
                Value val = vm_peek(vm);
                vm_set_global_slot(vm, slot, val);
                break;
            }

            case OP_ADD: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);

                if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                    vm_push(vm, make_number_value(left.as.number + right.as.number));
                } else if (left.type == VAL_STRING || right.type == VAL_STRING) {
                    char *lstr = value_to_display_string(left);
                    char *rstr = value_to_display_string(right);
                    size_t llen = strlen(lstr);
                    size_t rlen = strlen(rstr);

                    char *result = malloc(llen + rlen + 1);
                    memcpy(result, lstr, llen);
                    memcpy(result + llen, rstr, rlen + 1);
                    vm_push(vm, make_string_value_len(result, llen + rlen));
                    free(lstr);
                    free(rstr);
                    free(result);
                } else {
                    vm_raise_at(vm, op_ip, "'+' requires two numbers (or a string)");
                }
                break;
            }

            case OP_SUBTRACT: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                vm_push(vm, make_number_value(left.as.number - right.as.number));
                break;
            }

            case OP_MULTIPLY: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                vm_push(vm, make_number_value(left.as.number * right.as.number));
                break;
            }

            case OP_DIVIDE: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                if (right.as.number == 0.0) {
                    vm_raise_at(vm, op_ip, "division by zero");
                    break;
                }
                vm_push(vm, make_number_value(left.as.number / right.as.number));
                break;
            }

            case OP_MODULO: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                if (right.as.number == 0.0) {
                    vm_raise_at(vm, op_ip, "division by zero");
                    break;
                }
                vm_push(vm, make_number_value(fmod(left.as.number, right.as.number)));
                break;
            }

            case OP_NEGATE: {
                Value v = vm_pop(vm);
                v = to_number(v);
                vm_push(vm, make_number_value(-v.as.number));
                break;
            }

            case OP_EQUALS: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                vm_push(vm, make_bool_value(values_equal(left, right)));
                break;
            }

            case OP_NOT_EQUALS: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                vm_push(vm, make_bool_value(!values_equal(left, right)));
                break;
            }

            case OP_LESS_THAN: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                vm_push(vm, make_bool_value(left.as.number < right.as.number));
                break;
            }

            case OP_GREATER_THAN: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                vm_push(vm, make_bool_value(left.as.number > right.as.number));
                break;
            }

            case OP_LESS_THAN_EQUAL: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                vm_push(vm, make_bool_value(left.as.number <= right.as.number));
                break;
            }

            case OP_GREATER_THAN_EQUAL: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_number(left);
                right = to_number(right);
                vm_push(vm, make_bool_value(left.as.number >= right.as.number));
                break;
            }

            case OP_AND: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_bool(left);
                right = to_bool(right);
                vm_push(vm, make_bool_value(left.as.boolean && right.as.boolean));
                break;
            }

            case OP_OR: {
                Value right = vm_pop(vm);
                Value left = vm_pop(vm);
                left = to_bool(left);
                right = to_bool(right);
                vm_push(vm, make_bool_value(left.as.boolean || right.as.boolean));
                break;
            }

            case OP_NOT: {
                Value v = vm_pop(vm);
                v = to_bool(v);
                vm_push(vm, make_bool_value(!v.as.boolean));
                break;
            }

            case OP_INDEX: {
                Value index = vm_pop(vm);
                Value collection = vm_pop(vm);

                if (collection.type == VAL_ARRAY) {
                    if (index.type != VAL_NUMBER) {
                        vm_raise_at(vm, op_ip, "array index must be a number");
                        break;
                    }
                    if (index.as.number < 0 || index.as.number >= collection.as.array->count) {
                        vm_raise_at(vm, op_ip, "array index out of bounds");
                        break;
                    }
                    if (index.as.number != (double)(int)index.as.number) {
                        vm_raise_at(vm, op_ip, "array index must be an integer");
                        break;
                    }
                    int idx = (int)index.as.number;
                    vm_push(vm, collection.as.array->items[idx]);
                } else if (collection.type == VAL_STRING) {
                    if (index.type != VAL_NUMBER) {
                        vm_raise_at(vm, op_ip, "string index must be a number");
                        break;
                    }
                    size_t slen = (size_t)AS_OBJ_STRING(collection)->length;
                    if (index.as.number < 0 || index.as.number >= (double)slen) {
                        vm_raise_at(vm, op_ip, "string index out of bounds");
                        break;
                    }
                    if (index.as.number != (double)(int)index.as.number) {
                        vm_raise_at(vm, op_ip, "string index must be an integer");
                        break;
                    }
                    char ch_str[2] = { collection.as.string[(int)index.as.number], '\0' };
                    vm_push(vm, make_string_value_len(ch_str, 1));
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
                        vm_push(vm, make_none_value());
                        break;
                    }
                    vm_push(vm, out_val);
                } else {
                    vm_raise_at(vm, op_ip, "cannot index a non-collection value");
                    break;
                }
                break;
            }

            case OP_INDEX_SET: {
                Value val = vm_pop(vm);
                Value index = vm_pop(vm);
                Value collection = vm_pop(vm);

                if (collection.type == VAL_MAP) {
                    if (index.type == VAL_STRING) {
                        value_map_set_obj(collection.as.map, AS_OBJ_STRING(index), val);
                    } else {
                        char *kstr = value_to_display_string(index);
                        value_map_set(collection.as.map, kstr, val);
                        free(kstr);
                    }
                } else if (collection.type == VAL_ARRAY) {
                    if (index.type != VAL_NUMBER) {
                        vm_raise_at(vm, op_ip, "array index must be a number");
                        break;
                    }
                    if (index.as.number < 0 || index.as.number >= collection.as.array->count) {
                        vm_raise_at(vm, op_ip, "array index out of bounds");
                        break;
                    }
                    if (index.as.number != (double)(int)index.as.number) {
                        vm_raise_at(vm, op_ip, "array index must be an integer");
                        break;
                    }
                    int idx = (int)index.as.number;
                    collection.as.array->items[idx] = val;
                } else {
                    vm_raise_at(vm, op_ip, "cannot index set on non-collection value");
                    break;
                }
                break;
            }

            case OP_NEW_STRUCT: {
                uint32_t struct_idx = read_operand(vm);
                uint32_t arg_count = read_operand(vm);
                StructDef *def = chunk_get_runtime_struct(vm->chunk, (int)struct_idx);
                if (!def) {
                    for (uint32_t i = 0; i < arg_count; i++) vm_pop(vm);
                    vm_raise_at(vm, op_ip, "unknown struct type");
                    break;
                }
                if ((int)arg_count > def->field_count) {
                    for (uint32_t i = 0; i < arg_count; i++) vm_pop(vm);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "struct '%s' expects at most %d field argument(s), got %u",
                             def->name, def->field_count, arg_count);
                    vm_raise_at(vm, op_ip, msg);
                    break;
                }
                Value inst = make_instance_value(def);
                for (int i = (int)arg_count - 1; i >= 0; i--) {
                    inst.as.instance->fields[i] = vm_pop(vm);
                }
                vm_push(vm, inst);
                break;
            }

            case OP_GET_FIELD: {
                uint32_t f_idx = read_operand(vm);
                ObjString *fname = chunk_get_string_obj(vm->chunk, f_idx);
                Value obj = vm_pop(vm);
                if (obj.type == VAL_STRUCT && obj.as.instance) {
                    int slot = struct_def_find_field(obj.as.instance->struct_def, fname);
                    if (slot < 0) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "struct '%s' has no field '%s'",
                                 obj.as.instance->struct_def->name, fname->chars);
                        vm_raise_at(vm, op_ip, msg);
                        break;
                    }
                    vm_push(vm, obj.as.instance->fields[slot]);
                } else if (obj.type == VAL_MAP && obj.as.map) {
                    Value out_val = make_none_value();
                    if (value_map_get_obj(obj.as.map, fname, &out_val)) {
                        vm_push(vm, out_val);
                    } else {
                        vm_push(vm, make_none_value());
                    }
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "cannot access field '%s' on %s value",
                             fname->chars, value_type_name(obj));
                    vm_raise_at(vm, op_ip, msg);
                }
                break;
            }

            case OP_SET_FIELD: {
                uint32_t f_idx = read_operand(vm);
                ObjString *fname = chunk_get_string_obj(vm->chunk, f_idx);
                Value val = vm_pop(vm);
                Value obj = vm_pop(vm);
                if (obj.type == VAL_STRUCT && obj.as.instance) {
                    int slot = struct_def_find_field(obj.as.instance->struct_def, fname);
                    if (slot < 0) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "struct '%s' has no field '%s'",
                                 obj.as.instance->struct_def->name, fname->chars);
                        vm_raise_at(vm, op_ip, msg);
                        break;
                    }
                    obj.as.instance->fields[slot] = val;
                } else if (obj.type == VAL_MAP && obj.as.map) {
                    value_map_set_obj(obj.as.map, fname, val);
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "cannot set field '%s' on %s value",
                             fname->chars, value_type_name(obj));
                    vm_raise_at(vm, op_ip, msg);
                }
                break;
            }

            case OP_INVOKE: {
                uint32_t m_idx = read_operand(vm);
                uint32_t arg_count = read_operand(vm);
                ObjString *mname = chunk_get_string_obj(vm->chunk, m_idx);

                if (vm->sp < (int)arg_count + 1) {
                    vm_raise_at(vm, op_ip, "stack underflow in method call");
                    break;
                }

                Value receiver = vm->stack[vm->sp - (int)arg_count - 1];
                if (receiver.type != VAL_STRUCT || !receiver.as.instance) {
                    for (uint32_t i = 0; i <= arg_count; i++) vm_pop(vm);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "cannot call method '%s' on %s value",
                             mname->chars, value_type_name(receiver));
                    vm_raise_at(vm, op_ip, msg);
                    break;
                }

                StructDef *sdef = receiver.as.instance->struct_def;
                StructMethod *m = struct_def_find_method(sdef, mname);
                if (!m || m->func_idx < 0 || (size_t)m->func_idx >= vm->chunk->functions_len) {
                    for (uint32_t i = 0; i <= arg_count; i++) vm_pop(vm);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "struct '%s' has no method '%s'",
                             sdef->name, mname->chars);
                    vm_raise_at(vm, op_ip, msg);
                    break;
                }

                FunctionDef *func = &vm->chunk->functions[m->func_idx];
                if (func->param_count != (int)arg_count + 1) {
                    for (uint32_t i = 0; i <= arg_count; i++) vm_pop(vm);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "method '%s.%s' expects %d argument(s), got %u",
                             sdef->name, mname->chars, func->param_count - 1, arg_count);
                    vm_raise_at(vm, op_ip, msg);
                    break;
                }

                int base = vm->sp - ((int)arg_count + 1);
                if (!vm_push_call_frame(vm, vm->ip, op_ip, m->func_idx, base)) {
                    vm_raise_at(vm, op_ip, "call stack overflow");
                    break;
                }
                vm->ip = vm->chunk->code + func->code_offset;
                break;
            }

            case OP_COROUTINE: {
                uint32_t func_idx = read_operand(vm);
                uint32_t arg_count = read_operand(vm);
                Value *init_args = NULL;
                if (arg_count > 0) {
                    init_args = malloc(arg_count * sizeof(Value));
                    for (int i = (int)arg_count - 1; i >= 0; i--) {
                        init_args[i] = vm_pop(vm);
                    }
                }
                const char *fname = NULL;
                int resolved_idx = (int)func_idx;
                if (func_idx == 0xFFFFFFFFu) {
                    Value fn_val = vm_pop(vm);
                    if (fn_val.type != VAL_STRING) {
                        if (init_args) free(init_args);
                        vm_raise_at(vm, op_ip, "coroutine() first argument must be a function or function name");
                        break;
                    }
                    fname = fn_val.as.string;
                    resolved_idx = chunk_find_function(vm->chunk, fname);
                } else if (func_idx < vm->chunk->functions_len) {
                    fname = vm->chunk->functions[func_idx].name;
                }
                if (resolved_idx < 0 || (size_t)resolved_idx >= vm->chunk->functions_len) {
                    if (init_args) free(init_args);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "undefined coroutine function '%s'", fname ? fname : "<unknown>");
                    vm_raise_at(vm, op_ip, msg);
                    break;
                }
                Value co_val = make_coroutine_value(resolved_idx, fname, init_args, (int)arg_count);
                if (init_args) free(init_args);
                vm_push(vm, co_val);
                break;
            }

            case OP_YIELD: {
                Value yielded_val = vm_pop(vm);
                if (!vm->current_coro) {
                    vm_raise_at(vm, op_ip, "cannot yield from outside a coroutine");
                    break;
                }
                ObjCoroutine *co = vm->current_coro;
                co->state = CORO_SUSPENDED;
                vm_save_active_to_coro(vm, co);
                ObjCoroutine *caller = co->caller;
                co->caller = NULL;

                if (caller) {
                    vm_load_active_from_coro(vm, caller);
                    vm_push(vm, yielded_val);
                } else if (vm->has_saved_main) {
                    vm_load_active_from_main(vm);
                    vm_push(vm, yielded_val);
                    if (vm->ip == NULL) {
                        gc_restore_roots(saved_roots);
                        return 0;
                    }
                } else {
                    vm_push(vm, yielded_val);
                    gc_restore_roots(saved_roots);
                    return 0;
                }
                break;
            }

            case OP_RESUME: {
                uint32_t arg_count = read_operand(vm);
                if (arg_count < 1 || arg_count > 2) {
                    for (uint32_t i = 0; i < arg_count; i++) vm_pop(vm);
                    vm_raise_at(vm, op_ip, "resume() expects 1 or 2 arguments (coroutine [, value])");
                    break;
                }
                Value send_val = make_none_value();
                int has_send = 0;
                if (arg_count == 2) {
                    send_val = vm_pop(vm);
                    has_send = 1;
                }
                Value co_val = vm_pop(vm);
                if (co_val.type != VAL_COROUTINE || !co_val.as.coroutine) {
                    vm_raise_at(vm, op_ip, "resume() first argument must be a coroutine");
                    break;
                }
                ObjCoroutine *co = co_val.as.coroutine;
                int already_started = co->started;
                if (vm_enter_coroutine(vm, op_ip, co, has_send, send_val, 1)) {
                    if (already_started) {
                        /* Push send_val as the result of the suspended yield expression */
                        vm_push(vm, send_val);
                    }
                }
                break;
            }

            case OP_JUMP: {
                uint32_t addr = read_operand(vm);
                vm->ip = vm->chunk->code + addr;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint32_t addr = read_operand(vm);
                Value v = vm_pop(vm);
                v = to_bool(v);
                if (!v.as.boolean) {
                    vm->ip = vm->chunk->code + addr;
                }
                break;
            }

            case OP_TRY_BEGIN: {
                uint32_t catch_addr = read_operand(vm);
                vm_push_try_frame(vm, vm->chunk->code + catch_addr);
                break;
            }

            case OP_TRY_END: {
                if (vm->try_sp > 0) {
                    vm->try_sp--;
                    if (vm->current_coro) {
                        vm->current_coro->try_sp = vm->try_sp;
                    }
                }
                break;
            }

            case OP_OUTPUT: {
                Value v = vm_pop(vm);
                char *s = value_to_display_string(v);
                printf("%s\n", s);
                free(s);
                break;
            }

            case OP_INPUT: {
                char buffer[4096];
                if (fgets(buffer, sizeof(buffer), stdin)) {
                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len-1] == '\n') {
                        buffer[--len] = '\0';
                    }
                    if (len > 0 && buffer[len-1] == '\r') {
                        buffer[--len] = '\0';
                    }
                    vm_push(vm, make_string_value(buffer));
                } else {
                    vm_push(vm, make_string_value(""));
                }
                break;
            }

            case OP_LENGTH: {
                Value v = vm_pop(vm);
                if (v.type == VAL_STRING) {
                    vm_push(vm, make_number_value((double)AS_OBJ_STRING(v)->length));
                } else if (v.type == VAL_ARRAY) {
                    vm_push(vm, make_number_value((double)v.as.array->count));
                } else if (v.type == VAL_MAP) {
                    vm_push(vm, make_number_value((double)v.as.map->count));
                } else {
                    vm_push(vm, make_number_value(0.0));
                }
                break;
            }

            case OP_LOWERCASE: {
                Value v = vm_pop(vm);
                if (v.type == VAL_STRING) {
                    size_t len = (size_t)AS_OBJ_STRING(v)->length;
                    char *result = malloc(len + 1);
                    memcpy(result, v.as.string, len + 1);
                    for (char *p = result; *p; p++) {
                        if (*p >= 'A' && *p <= 'Z') {
                            *p = (char)(*p - 'A' + 'a');
                        }
                    }
                    vm_push(vm, make_string_value_len(result, len));
                    free(result);
                } else {
                    vm_push(vm, v);
                }
                break;
            }

            case OP_TRIM: {
                Value v = vm_pop(vm);
                if (v.type == VAL_STRING) {
                    const char *str = v.as.string;
                    const char *start = str;
                    const char *end = str + strlen(str) - 1;

                    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
                    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;

                    size_t len = (end >= start) ? (size_t)(end - start + 1) : 0;
                    vm_push(vm, make_string_value_len(start, len));
                } else {
                    vm_push(vm, v);
                }
                break;
            }

            case OP_NUMBER_CAST: {
                Value v = vm_pop(vm);
                if (v.type == VAL_NUMBER) {
                    vm_push(vm, v);
                    break;
                }
                if (v.type != VAL_STRING) {
                    vm_raise_at(vm, op_ip, "number() expects a string or number argument");
                    break;
                }
                char *end;
                double result = strtod(v.as.string, &end);
                while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
                if (end == v.as.string || *end != '\0') {
                    vm_raise_at(vm, op_ip, "number(): expected a valid number");
                    break;
                }
                vm_push(vm, make_number_value(result));
                break;
            }

            case OP_UPPERCASE: {
                Value v = vm_pop(vm);
                if (v.type == VAL_STRING) {
                    size_t len = (size_t)AS_OBJ_STRING(v)->length;
                    char *result = malloc(len + 1);
                    memcpy(result, v.as.string, len + 1);
                    for (char *p = result; *p; p++) {
                        if (*p >= 'a' && *p <= 'z') {
                            *p = (char)(*p - 'a' + 'A');
                        }
                    }
                    vm_push(vm, make_string_value_len(result, len));
                    free(result);
                } else {
                    vm_push(vm, v);
                }
                break;
            }

            case OP_STRING_CAST: {
                Value v = vm_pop(vm);
                char *s = value_to_display_string(v);
                vm_push(vm, make_string_value(s));
                free(s);
                break;
            }

            case OP_PUSH_BACK: {
                Value val = vm_pop(vm);
                Value arr = vm_pop(vm);
                if (arr.type != VAL_ARRAY) {
                    vm_raise_at(vm, op_ip, "push() first argument must be an array");
                    break;
                }
                value_array_push(arr.as.array, val);
                vm_push(vm, arr);
                break;
            }

            case OP_TYPE_OF: {
                Value v = vm_pop(vm);
                vm_push(vm, make_string_value(value_type_name(v)));
                break;
            }

            case OP_CALL_BUILTIN: {
                uint32_t name_idx = read_operand(vm);
                uint32_t arg_count = read_operand(vm);
                const char *bname = (name_idx < vm->chunk->strings_len) ? vm->chunk->strings[name_idx] : "";
                Value *args = NULL;
                if (arg_count > 0) {
                    args = malloc(arg_count * sizeof(Value));
                    for (int i = (int)arg_count - 1; i >= 0; i--) {
                        args[i] = vm_pop(vm);
                    }
                }
                Value out;
                const char *builtin_error = NULL;
                builtins_set_active_owner(vm->host_owner);
                int status = call_builtin(bname, args, (int)arg_count, &out, &builtin_error);
                if (args) free(args);
                if (status == 1) {
                    vm_push(vm, out);
                } else if (status == -1) {
                    vm_raise_at(vm, op_ip, builtin_error ? builtin_error : "builtin error");
                } else {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "undefined function '%s'", bname);
                    vm_raise_at(vm, op_ip, msg);
                }
                break;
            }

            case OP_POP: {
                vm_pop(vm);
                break;
            }

            case OP_CALL: {
                uint32_t func_idx = read_operand(vm);

                if (func_idx < vm->chunk->functions_len) {
                    FunctionDef *func = &vm->chunk->functions[func_idx];
                    int base = vm->sp - func->param_count;
                    if (!vm_push_call_frame(vm, vm->ip, op_ip, (int)func_idx, base)) {
                        vm_raise_at(vm, op_ip, "call stack overflow");
                        break;
                    }
                    vm->ip = vm->chunk->code + func->code_offset;
                } else {
                    vm_raise_at(vm, op_ip, "call to undefined function");
                }
                break;
            }

            case OP_RETURN: {
                Value return_val = vm_pop(vm);

                if (vm->call_sp > 0) {
                    vm->call_sp--;
                    if (vm->current_coro) {
                        vm->current_coro->call_sp = vm->call_sp;
                    }
                    CallFrame frame = vm->call_stack[vm->call_sp];
                    if (frame.return_ip == NULL && vm->current_coro != NULL) {
                        /* Coroutine entry function returned -> coroutine is now dead */
                        ObjCoroutine *co = vm->current_coro;
                        co->state = CORO_DEAD;
                        vm->sp = 0;
                        vm->try_sp = 0;
                        vm_save_active_to_coro(vm, co);
                        ObjCoroutine *caller = co->caller;
                        co->caller = NULL;

                        if (caller) {
                            vm_load_active_from_coro(vm, caller);
                            vm_push(vm, return_val);
                        } else if (vm->has_saved_main) {
                            vm_load_active_from_main(vm);
                            vm_push(vm, return_val);
                            if (vm->ip == NULL) {
                                gc_restore_roots(saved_roots);
                                return 0;
                            }
                        } else {
                            vm_push(vm, return_val);
                            gc_restore_roots(saved_roots);
                            return 0;
                        }
                        break;
                    }
                    vm->ip = frame.return_ip;
                    vm->sp = frame.local_var_base;
                    vm->try_sp = frame.try_sp;
                    if (vm->current_coro) {
                        vm->current_coro->sp = vm->sp;
                        vm->current_coro->try_sp = vm->try_sp;
                    }
                    vm_push(vm, return_val);
                } else {
                    vm_push(vm, return_val);
                    gc_restore_roots(saved_roots);
                    return 0;
                }
                break;
            }

            case OP_HALT:
                gc_restore_roots(saved_roots);
                return 0;

            default:
                fprintf(stderr, "Unknown opcode: %d\n", op);
                gc_restore_roots(saved_roots);
                return 1;
        }

        gc_restore_roots(saved_roots);
    }

    return 0;
}

int vm_call_function(VM *vm, int func_idx, int arg_count, Value *args, Value *out) {
    if (!vm || !vm->chunk || func_idx < 0 || (size_t)func_idx >= vm->chunk->functions_len) {
        return -1;
    }
    FunctionDef *func = &vm->chunk->functions[func_idx];
    if (func->param_count != arg_count) {
        return -1;
    }
    vm->had_error = 0;
    int saved_sp = vm->sp;
    int saved_call_sp = vm->call_sp;
    int saved_try_sp = vm->try_sp;
    for (int i = 0; i < arg_count; i++) {
        vm_push(vm, args[i]);
    }
    /* Sentinel return_ip == NULL with current_coro == NULL won't trigger if call_sp == 0,
     * so let's point return_ip at an OP_HALT byte at the end of chunk->code. */
    uint8_t *halt_ip = vm->chunk->code + vm->chunk->code_len - 1;
    if (!vm_push_call_frame(vm, halt_ip, vm->chunk->code + func->code_offset, func_idx, saved_sp)) {
        vm->sp = saved_sp;
        return -1;
    }
    vm->ip = vm->chunk->code + func->code_offset;
    int rc = vm_execute(vm);
    if (rc != 0) {
        vm->sp = saved_sp;
        vm->call_sp = saved_call_sp;
        vm->try_sp = saved_try_sp;
        return -1;
    }
    Value ret = (vm->sp > saved_sp) ? vm_pop(vm) : make_none_value();
    vm->sp = saved_sp;
    if (out) {
        *out = ret;
    }
    return 0;
}

int vm_resume_coroutine(VM *vm, ObjCoroutine *co, int has_send_val, Value send_val, Value *out) {
    if (!vm || !co) return -1;
    vm->had_error = 0;
    int already_started = co->started;
    if (!vm_enter_coroutine(vm, vm->ip, co, has_send_val, send_val, 0)) {
        return -1;
    }
    if (already_started) {
        vm_push(vm, send_val);
    }
    int rc = vm_execute(vm);
    if (rc != 0 || vm->had_error) {
        return -1;
    }
    Value res = (vm->sp > 0) ? vm_pop(vm) : make_none_value();
    if (out) *out = res;
    int status = (co->state == CORO_SUSPENDED) ? 1 : 0;
    vm->current_coro = NULL;
    return status;
}