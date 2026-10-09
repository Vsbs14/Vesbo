#ifndef VESBO_RAYLIB_BINDINGS_H
#define VESBO_RAYLIB_BINDINGS_H

#include "value.h"

int is_raylib_builtin(const char *name);
int call_raylib_builtin(const char *name, Value *args, int arg_count, Value *out,
                        const char **error_message);

#endif
