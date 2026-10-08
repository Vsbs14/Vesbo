#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../include/vm.h"
#include "../include/value.h"
#include "../include/builtins.h"

VM *vm_new(Chunk *chunk) {
    VM *vm = malloc(sizeof(VM));
    if (!vm) return NULL;

    vm->chunk = chunk;
    vm->ip = chunk->code;

    vm->stack = malloc(VM_STACK_SIZE * sizeof(Value));
    if (!vm->stack) {
        free(vm);
        return NULL;
    }
    vm->sp = 0;

    vm->call_stack = malloc(VM_CALL_STACK_SIZE * sizeof(CallFrame));
    if (!vm->call_stack) {
        free(vm->stack);
        free(vm);
        return NULL;
    }
    vm->call_sp = 0;

    vm->globals = malloc(64 * sizeof(GlobalVar));
    if (!vm->globals) {
        free(vm->stack);
        free(vm->call_stack);
        free(vm);
        return NULL;
    }
    vm->global_cap = 64;
    vm->global_count = 0;

    vm->try_stack = malloc(VM_TRY_STACK_SIZE * sizeof(TryFrame));
    if (!vm->try_stack) {
        free(vm->stack);
        free(vm->call_stack);
        free(vm->globals);
        free(vm);
        return NULL;
    }
    vm->try_sp = 0;

    vm->had_error = 0;
    vm->error_message[0] = '\0';

    return vm;
}

void vm_free(VM *vm) {
    if (!vm) return;
    free(vm->stack);
    free(vm->call_stack);
    for (int i = 0; i < vm->global_count; i++) {
        free(vm->globals[i].name);
    }
    free(vm->globals);
    free(vm->try_stack);
    free(vm);
}

void vm_push(VM *vm, Value val) {
    if (vm->sp >= VM_STACK_SIZE) {
        fprintf(stderr, "Runtime error: stack overflow\n");
        vm->had_error = 1;
        return;
    }
    vm->stack[vm->sp++] = val;
}

Value vm_pop(VM *vm) {
    if (vm->sp <= 0) {
        fprintf(stderr, "Runtime error: stack underflow\n");
        vm->had_error = 1;
        return make_none_value();
    }
    return vm->stack[--vm->sp];
}

Value vm_peek(VM *vm) {
    if (vm->sp > 0) {
        return vm->stack[vm->sp - 1];
    }
    return make_none_value();
}

static uint32_t read_operand(VM *vm) {
    uint32_t value = 0;
    value |= ((uint32_t)*vm->ip++) << 24;
    value |= ((uint32_t)*vm->ip++) << 16;
    value |= ((uint32_t)*vm->ip++) << 8;
    value |= (uint32_t)*vm->ip++;
    return value;
}

/* Look up a global by name, returning its slot index or -1 */
static int global_find(VM *vm, const char *name) {
    for (int i = 0; i < vm->global_count; i++) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void global_set(VM *vm, const char *name, Value value) {
    int idx = global_find(vm, name);
    if (idx >= 0) {
        vm->globals[idx].value = value;
        return;
    }

    if (vm->global_count >= vm->global_cap) {
        vm->global_cap *= 2;
        vm->globals = realloc(vm->globals, vm->global_cap * sizeof(GlobalVar));
    }

    char *name_copy = malloc(strlen(name) + 1);
    strcpy(name_copy, name);
    vm->globals[vm->global_count].name = name_copy;
    vm->globals[vm->global_count].value = value;
    vm->global_count++;
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

/* Raise a runtime error. If a try/catch handler is active, unwind the
 * stack to it, push the error message as a string, and jump to the catch
 * target. Otherwise flag the VM as errored so vm_execute can report and
 * halt, mirroring the tree-walking interpreter's uncaught-error behavior. */
static void vm_raise(VM *vm, const char *message) {
    if (vm->try_sp > 0) {
        TryFrame frame = vm->try_stack[--vm->try_sp];
        vm->sp = frame.sp;
        vm->call_sp = frame.call_sp;
        vm->ip = frame.catch_ip;
        vm_push(vm, make_string_value(message));
        return;
    }

    vm->had_error = 1;
    strncpy(vm->error_message, message, sizeof(vm->error_message) - 1);
    vm->error_message[sizeof(vm->error_message) - 1] = '\0';
}

/* Current call frame's local variable base (0 at top level) */
static int current_local_base(VM *vm) {
    if (vm->call_sp > 0) {
        return vm->call_stack[vm->call_sp - 1].local_var_base;
    }
    return 0;
}

int vm_execute(VM *vm) {
    while (1) {
        if (vm->had_error) {
            fprintf(stderr, "Runtime error: %s\n", vm->error_message);
            return 1;
        }

        OpCode op = (OpCode)*vm->ip++;

        switch (op) {
            case OP_PUSH_NUMBER: {
                uint32_t idx = read_operand(vm);
                vm_push(vm, make_number_value(vm->chunk->numbers[idx]));
                break;
            }

            case OP_PUSH_STRING: {
                uint32_t idx = read_operand(vm);
                vm_push(vm, make_string_value(vm->chunk->strings[idx]));
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
                    /* Collect elements in temporary array - they're on stack in order
                       but we need to pop them (in reverse) then add in correct order */
                    Value *temp = malloc(count * sizeof(Value));

                    /* Pop all elements - this reverses them */
                    for (uint32_t i = 0; i < count; i++) {
                        temp[i] = vm_pop(vm);
                    }

                    /* Push them back in reverse order to get original order */
                    for (int i = count - 1; i >= 0; i--) {
                        value_array_push(&arr.as.array, temp[i]);
                    }

                    free(temp);
                }

                vm_push(vm, arr);
                break;
            }

            case OP_DEFINE_LOCAL: {
                /* Locals live directly on the value stack, at
                 * (frame base + frame-relative index). The value to define
                 * is on top of the stack from the preceding expression.
                 * A slot can be *redefined* - e.g. a "var" inside a loop
                 * body runs through DEFINE_LOCAL again on every iteration,
                 * for the same frame-relative index - so this must behave
                 * like an assignment into that slot (extending the stack
                 * only the first time), never an unconditional push, or
                 * the stack grows without bound across iterations. */
                uint32_t idx = read_operand(vm);
                int slot = current_local_base(vm) + (int)idx;
                Value val = vm_pop(vm);
                if (slot == vm->sp) {
                    vm_push(vm, val);
                } else if (slot >= 0 && slot < vm->sp) {
                    vm->stack[slot] = val;
                } else {
                    /* Slot is further ahead than the stack currently
                     * reaches (shouldn't normally happen) - pad up to it. */
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
                uint32_t idx = read_operand(vm);
                const char *name = vm->chunk->strings[idx];
                int gidx = global_find(vm, name);
                if (gidx >= 0) {
                    vm_push(vm, vm->globals[gidx].value);
                } else {
                    vm_push(vm, make_none_value());
                }
                break;
            }

            case OP_SET_GLOBAL: {
                uint32_t idx = read_operand(vm);
                const char *name = vm->chunk->strings[idx];
                Value val = vm_peek(vm);
                global_set(vm, name, val);
                /* Leave the value on the stack; codegen emits an explicit
                 * OP_POP afterwards for statement contexts. */
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

                    char *result = malloc(strlen(lstr) + strlen(rstr) + 1);
                    strcpy(result, lstr);
                    strcat(result, rstr);
                    vm_push(vm, make_string_value(result));
                    free(lstr);
                    free(rstr);
                    free(result);
                } else {
                    vm_raise(vm, "'+' requires two numbers (or a string)");
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
                    vm_raise(vm, "division by zero");
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
                    vm_raise(vm, "division by zero");
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
                        vm_raise(vm, "array index must be a number");
                        break;
                    }
                    if (index.as.number < 0 || index.as.number >= collection.as.array.count) {
                        vm_raise(vm, "array index out of bounds");
                        break;
                    }
                    if (index.as.number != (double)(int)index.as.number) {
                        vm_raise(vm, "array index must be an integer");
                        break;
                    }
                    int idx = (int)index.as.number;
                    vm_push(vm, collection.as.array.items[idx]);
                } else if (collection.type == VAL_STRING) {
                    if (index.type != VAL_NUMBER) {
                        vm_raise(vm, "string index must be a number");
                        break;
                    }
                    size_t slen = strlen(collection.as.string);
                    if (index.as.number < 0 || index.as.number >= slen) {
                        vm_raise(vm, "string index out of bounds");
                        break;
                    }
                    if (index.as.number != (double)(int)index.as.number) {
                        vm_raise(vm, "string index must be an integer");
                        break;
                    }
                    char ch_str[2] = { collection.as.string[(int)index.as.number], '\0' };
                    vm_push(vm, make_string_value(ch_str));
                } else {
                    vm_raise(vm, "cannot index a non-collection value");
                    break;
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
                if (vm->try_sp < VM_TRY_STACK_SIZE) {
                    vm->try_stack[vm->try_sp].catch_ip = vm->chunk->code + catch_addr;
                    vm->try_stack[vm->try_sp].sp = vm->sp;
                    vm->try_stack[vm->try_sp].call_sp = vm->call_sp;
                    vm->try_sp++;
                }
                break;
            }

            case OP_TRY_END: {
                if (vm->try_sp > 0) {
                    vm->try_sp--;
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
                    vm_push(vm, make_number_value((double)strlen(v.as.string)));
                } else if (v.type == VAL_ARRAY) {
                    vm_push(vm, make_number_value((double)v.as.array.count));
                } else {
                    vm_push(vm, make_number_value(0.0));
                }
                break;
            }

            case OP_LOWERCASE: {
                Value v = vm_pop(vm);
                if (v.type == VAL_STRING) {
                    char *result = malloc(strlen(v.as.string) + 1);
                    strcpy(result, v.as.string);
                    for (char *p = result; *p; p++) {
                        if (*p >= 'A' && *p <= 'Z') {
                            *p = *p - 'A' + 'a';
                        }
                    }
                    vm_push(vm, make_string_value(result));
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

                    while (*start == ' ' || *start == '\t' || *start == '\n') start++;
                    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\n')) end--;

                    size_t len = (end >= start) ? (size_t)(end - start + 1) : 0;
                    char *result = malloc(len + 1);
                    strncpy(result, start, len);
                    result[len] = '\0';

                    vm_push(vm, make_string_value(result));
                    free(result);
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
                    vm_raise(vm, "number() expects a string or number argument");
                    break;
                }
                char *end;
                double result = strtod(v.as.string, &end);
                while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
                if (end == v.as.string || *end != '\0') {
                    vm_raise(vm, "number(): expected a valid number");
                    break;
                }
                vm_push(vm, make_number_value(result));
                break;
            }

            case OP_UPPERCASE: {
                Value v = vm_pop(vm);
                if (v.type == VAL_STRING) {
                    char *result = malloc(strlen(v.as.string) + 1);
                    strcpy(result, v.as.string);
                    for (char *p = result; *p; p++) {
                        if (*p >= 'a' && *p <= 'z') {
                            *p = *p - 'a' + 'A';
                        }
                    }
                    vm_push(vm, make_string_value(result));
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
                    vm_raise(vm, "push() first argument must be an array");
                    break;
                }
                value_array_push(&arr.as.array, val);
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
                int status = call_builtin(bname, args, (int)arg_count, &out, &builtin_error);
                if (args) free(args);
                if (status == 1) {
                    vm_push(vm, out);
                } else if (status == -1) {
                    vm_raise(vm, builtin_error ? builtin_error : "builtin error");
                } else {
                    vm_raise(vm, "unknown builtin function");
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

                    if (vm->call_sp < VM_CALL_STACK_SIZE) {
                        /* Save current state */
                        vm->call_stack[vm->call_sp].return_ip = vm->ip;
                        vm->call_stack[vm->call_sp].local_var_base = vm->sp - func->param_count;
                        vm->call_stack[vm->call_sp].try_sp = vm->try_sp;
                        vm->call_sp++;

                        /* Jump to function */
                        vm->ip = vm->chunk->code + func->code_offset;
                    } else {
                        vm_raise(vm, "call stack overflow");
                    }
                } else {
                    vm_raise(vm, "call to undefined function");
                }
                break;
            }

            case OP_RETURN: {
                Value return_val = vm_pop(vm);  /* Return value is on stack */

                if (vm->call_sp > 0) {
                    /* Restore previous state */
                    vm->call_sp--;
                    CallFrame frame = vm->call_stack[vm->call_sp];
                    vm->ip = frame.return_ip;
                    vm->sp = frame.local_var_base;
                    vm->try_sp = frame.try_sp; /* drop any handlers set up inside the call */

                    /* Push return value back */
                    vm_push(vm, return_val);
                } else {
                    /* No call stack - end of program */
                    return 0;
                }
                break;
            }

            case OP_HALT:
                return 0;

            default:
                fprintf(stderr, "Unknown opcode: %d\n", op);
                return 1;
        }
    }

    return 0;
}