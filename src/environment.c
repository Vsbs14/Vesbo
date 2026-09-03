#include "../include/environment.h"
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

static void scope_init(Scope *scope) {
    scope->names = NULL;
    scope->values = NULL;
    scope->count = 0;
    scope->capacity = 0;
}

static void scope_free(Scope *scope) {
    for (int i = 0; i < scope->count; i++) {
        free(scope->names[i]);
    }
    free(scope->names);
    free(scope->values);
}

static void scope_declare(Scope *scope, const char *name, Value value) {
    if (scope->count >= scope->capacity) {
        scope->capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        scope->names = realloc(scope->names, sizeof(char *) * scope->capacity);
        scope->values = realloc(scope->values, sizeof(Value) * scope->capacity);
    }
    scope->names[scope->count] = copy_string(name);
    scope->values[scope->count] = value;
    scope->count++;
}

// Returns index of `name` in scope, or -1 if not present.
static int scope_find(Scope *scope, const char *name) {
    for (int i = scope->count - 1; i >= 0; i--) {
        if (strcmp(scope->names[i], name) == 0) return i;
    }
    return -1;
}

void env_init(Environment *env) {
    env->scopes = NULL;
    env->count = 0;
    env->capacity = 0;
    env_push_scope(env); // scope 0: global
}

void env_free(Environment *env) {
    while (env->count > 0) {
        env->count--;
        scope_free(&env->scopes[env->count]);
    }
    free(env->scopes);
    env->scopes = NULL;
    env->capacity = 0;
}

void env_push_scope(Environment *env) {
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 4 : env->capacity * 2;
        env->scopes = realloc(env->scopes, sizeof(Scope) * env->capacity);
    }
    scope_init(&env->scopes[env->count]);
    env->count++;
}

void env_pop_scope(Environment *env) {
    if (env->count <= 1) return; // never pop the global scope
    env->count--;
    scope_free(&env->scopes[env->count]);
}

void env_declare(Environment *env, const char *name, Value value, int is_global) {
    if (is_global) {
        scope_declare(&env->scopes[0], name, value);
    } else {
        scope_declare(&env->scopes[env->count - 1], name, value);
    }
}

int env_get(Environment *env, const char *name, Value *out) {
    for (int i = env->count - 1; i >= 0; i--) {
        int idx = scope_find(&env->scopes[i], name);
        if (idx != -1) {
            *out = env->scopes[i].values[idx];
            return 1;
        }
    }
    return 0;
}

int env_set(Environment *env, const char *name, Value value) {
    for (int i = env->count - 1; i >= 0; i--) {
        int idx = scope_find(&env->scopes[i], name);
        if (idx != -1) {
            env->scopes[i].values[idx] = value;
            return 1;
        }
    }
    return 0;
}