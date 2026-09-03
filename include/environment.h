#ifndef VESBO_ENVIRONMENT_H
#define VESBO_ENVIRONMENT_H

#include "value.h"

typedef struct {
    char **names;
    Value *values;
    int count;
    int capacity;
} Scope;

typedef struct {
    Scope *scopes;
    int count;
    int capacity;
} Environment;

void env_init(Environment *env);
void env_free(Environment *env);
void env_push_scope(Environment *env);
void env_pop_scope(Environment *env);

void env_declare(Environment *env, const char *name, Value value, int is_global);

int env_get(Environment *env, const char *name, Value *out);

int env_set(Environment *env, const char *name, Value value);

#endif