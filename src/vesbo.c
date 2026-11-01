#include "../include/vesbo.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/builtins.h"
#include "../include/module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    VesboNativeFn fn;
    VesboVM *vm;
} HostNativeBinding;

struct VesboVM {
    CodeGen *codegen;
    Chunk *chunk;
    VM *vm;
    StmtList ast;
    int has_ast;
    HostNativeBinding *bindings;
    int binding_count;
    int binding_cap;
    Value default_coro;
    int has_pending_error;
    char pending_error[512];
};

static int host_native_trampoline(void *user_data, int argc, Value *argv, Value *out, const char **error_message) {
    HostNativeBinding *b = (HostNativeBinding *)user_data;
    if (!b || !b->fn || !b->vm) return 0;
    b->vm->has_pending_error = 0;
    Value res = b->fn(b->vm, argc, argv);
    if (b->vm->has_pending_error) {
        *error_message = b->vm->pending_error;
        return -1;
    }
    *out = res;
    return 1;
}

VesboVM *vesbo_new(void) {
    VesboVM *vvm = malloc(sizeof(VesboVM));
    if (!vvm) return NULL;
    memset(vvm, 0, sizeof(VesboVM));
    vvm->default_coro = make_none_value();
    return vvm;
}

void vesbo_free(VesboVM *vvm) {
    if (!vvm) return;
    if (vvm->vm) {
        vm_free(vvm->vm);
        vvm->vm = NULL;
    }
    if (vvm->codegen) {
        codegen_free(vvm->codegen);
        vvm->codegen = NULL;
        vvm->chunk = NULL;
    }
    if (vvm->has_ast) {
        free_stmt_list(&vvm->ast);
        vvm->has_ast = 0;
    }
    for (int i = 0; i < vvm->binding_count; i++) {
        free(vvm->bindings[i].name);
    }
    free(vvm->bindings);
    builtins_clear_natives_owned(vvm);
    free(vvm);
}

void vesbo_register_native(VesboVM *vvm, const char *name, VesboNativeFn fn) {
    if (!vvm || !name || !fn) return;
    for (int i = 0; i < vvm->binding_count; i++) {
        if (strcmp(vvm->bindings[i].name, name) == 0) {
            vvm->bindings[i].fn = fn;
            builtins_register_native_owned(vvm, name, host_native_trampoline, &vvm->bindings[i]);
            return;
        }
    }
    if (vvm->binding_count >= vvm->binding_cap) {
        int new_cap = vvm->binding_cap == 0 ? 16 : vvm->binding_cap * 2;
        HostNativeBinding * nb = realloc(vvm->bindings, (size_t)new_cap * sizeof(HostNativeBinding));
        if (!nb) return;
        vvm->bindings = nb;
        vvm->binding_cap = new_cap;
        /* Re-register pointers in builtins table after realloc */
        for (int i = 0; i < vvm->binding_count; i++) {
            builtins_register_native_owned(vvm, vvm->bindings[i].name, host_native_trampoline, &vvm->bindings[i]);
        }
    }
    int idx = vvm->binding_count++;
    vvm->bindings[idx].name = malloc(strlen(name) + 1);
    strcpy(vvm->bindings[idx].name, name);
    vvm->bindings[idx].fn = fn;
    vvm->bindings[idx].vm = vvm;
    builtins_register_native_owned(vvm, name, host_native_trampoline, &vvm->bindings[idx]);
}

void vesbo_error(VesboVM *vvm, const char *message) {
    if (!vvm) return;
    vvm->has_pending_error = 1;
    strncpy(vvm->pending_error, message ? message : "native error", sizeof(vvm->pending_error) - 1);
    vvm->pending_error[sizeof(vvm->pending_error) - 1] = '\0';
}

int vesbo_load_string(VesboVM *vvm, const char *source, const char *filename) {
    if (!vvm || !source) return 1;

    if (vvm->vm) {
        vm_free(vvm->vm);
        vvm->vm = NULL;
    }
    if (vvm->codegen) {
        codegen_free(vvm->codegen);
        vvm->codegen = NULL;
        vvm->chunk = NULL;
    }
    if (vvm->has_ast) {
        free_stmt_list(&vvm->ast);
        vvm->has_ast = 0;
    }

    module_system_init();
    module_set_current_file(filename ? filename : "<embedded>");
    lexer_init(source);
    vvm->ast = parse_program();
    vvm->has_ast = 1;

    vvm->codegen = codegen_new();
    if (!vvm->codegen) return 1;

    vvm->chunk = codegen_compile(vvm->codegen, vvm->ast.items, vvm->ast.count);
    if (!vvm->chunk) return 1;

    vvm->vm = vm_new(vvm->chunk);
    if (!vvm->vm) return 1;
    vvm->vm->host_owner = vvm;

    int rc = vm_execute(vvm->vm);
    return rc;
}

int vesbo_load_file(VesboVM *vvm, const char *filepath) {
    if (!vvm || !filepath) return 1;
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        snprintf(vvm->pending_error, sizeof(vvm->pending_error), "could not open file: %s", filepath);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return 1;
    }
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return 1;
    }
    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    buf[read_bytes] = '\0';
    fclose(f);

    int rc = vesbo_load_string(vvm, buf, filepath);
    free(buf);
    return rc;
}

int vesbo_load(VesboVM *vvm, const char *source_or_path) {
    if (!vvm || !source_or_path) return 1;
    /* If source_or_path is a single line and names an existing file, load it as a file */
    if (strchr(source_or_path, '\n') == NULL) {
        FILE *f = fopen(source_or_path, "rb");
        if (f) {
            fclose(f);
            return vesbo_load_file(vvm, source_or_path);
        }
    }
    return vesbo_load_string(vvm, source_or_path, "<embedded>");
}

Value vesbo_coroutine(VesboVM *vvm, const char *func_name, int argc, Value *argv) {
    if (!vvm || !vvm->vm || !vvm->chunk || !func_name) return make_none_value();
    int func_idx = chunk_find_function(vvm->chunk, func_name);
    if (func_idx < 0) return make_none_value();
    Value co = make_coroutine_value(func_idx, func_name, argv, argc);
    if (co.type == VAL_COROUTINE && co.as.coroutine) {
        vm_pin_coroutine(vvm->vm, co.as.coroutine);
    }
    vvm->default_coro = co;
    return co;
}

static ObjCoroutine *resolve_coroutine_target(VesboVM *vvm, Value coroutine) {
    if (!vvm || !vvm->vm) return NULL;
    if (coroutine.type == VAL_COROUTINE && coroutine.as.coroutine) {
        return coroutine.as.coroutine;
    }
    if (coroutine.type == VAL_STRING && coroutine.as.string) {
        /* First check if a global variable with this name holds a coroutine */
        Value gval;
        if (vm_get_global_by_name(vvm->vm, coroutine.as.string, &gval) &&
            gval.type == VAL_COROUTINE && gval.as.coroutine) {
            return gval.as.coroutine;
        }
        /* Otherwise check if default_coro matches this function name */
        if (vvm->default_coro.type == VAL_COROUTINE && vvm->default_coro.as.coroutine &&
            vvm->default_coro.as.coroutine->func_name &&
            strcmp(vvm->default_coro.as.coroutine->func_name, coroutine.as.string) == 0 &&
            vvm->default_coro.as.coroutine->state != CORO_DEAD) {
            return vvm->default_coro.as.coroutine;
        }
        /* Otherwise create a new coroutine for that function name */
        Value co = vesbo_coroutine(vvm, coroutine.as.string, 0, NULL);
        if (co.type == VAL_COROUTINE) return co.as.coroutine;
    }
    if (coroutine.type == VAL_NONE) {
        if (vvm->default_coro.type == VAL_COROUTINE && vvm->default_coro.as.coroutine) {
            return vvm->default_coro.as.coroutine;
        }
        /* Search globals for any coroutine */
        for (int i = 0; i < vvm->vm->global_count; i++) {
            if (vvm->vm->global_defined[i] && vvm->vm->globals[i].type == VAL_COROUTINE) {
                return vvm->vm->globals[i].as.coroutine;
            }
        }
    }
    return NULL;
}

int vesbo_resume(VesboVM *vvm, Value coroutine, Value *out) {
    ObjCoroutine *co = resolve_coroutine_target(vvm, coroutine);
    if (!co) return -1;
    return vm_resume_coroutine(vvm->vm, co, 0, make_none_value(), out);
}

int vesbo_resume_with(VesboVM *vvm, Value coroutine, Value send_val, Value *out) {
    ObjCoroutine *co = resolve_coroutine_target(vvm, coroutine);
    if (!co) return -1;
    return vm_resume_coroutine(vvm->vm, co, 1, send_val, out);
}

int vesbo_call(VesboVM *vvm, const char *func_name, int argc, Value *argv, Value *out) {
    if (!vvm || !vvm->vm || !vvm->chunk || !func_name) return -1;
    int func_idx = chunk_find_function(vvm->chunk, func_name);
    if (func_idx < 0) return -1;
    return vm_call_function(vvm->vm, func_idx, argc, argv, out);
}

Value vesbo_get_global(VesboVM *vvm, const char *name) {
    if (!vvm || !vvm->vm || !name) return make_none_value();
    Value out = make_none_value();
    vm_get_global_by_name(vvm->vm, name, &out);
    return out;
}

void vesbo_set_global(VesboVM *vvm, const char *name, Value val) {
    if (!vvm || !vvm->vm || !name) return;
    vm_set_global_by_name(vvm->vm, name, val);
}

const char *vesbo_last_error(VesboVM *vvm) {
    if (!vvm) return "";
    if (vvm->vm && vvm->vm->error_message[0]) return vvm->vm->error_message;
    return vvm->pending_error;
}

const char *vesbo_last_stack_trace(VesboVM *vvm) {
    if (vvm && vvm->vm && vvm->vm->stack_trace[0]) return vvm->vm->stack_trace;
    return vesbo_get_last_stack_trace();
}
