#ifndef VESBO_BUILTINS_H
#define VESBO_BUILTINS_H

#include "value.h"

typedef int (*VesboNativeCallback)(void *user_data, int argc, Value *argv, Value *out, const char **error_message);

// Register a host native C function
void builtins_register_native(const char *name, VesboNativeCallback fn, void *user_data);
void builtins_register_native_owned(void *owner, const char *name, VesboNativeCallback fn, void *user_data);

// Clear registered host native functions
void builtins_clear_natives(void);
void builtins_clear_natives_owned(void *owner);
void builtins_set_active_owner(void *owner);

// Returns 1 if name is a known built-in or registered native function, 0 otherwise
int is_builtin(const char *name);

// Returns 1 on success, 0 when name is not a built-in, and -1 when the
// built-in reports a runtime error. On failure, *error_message is set.
int call_builtin(const char *name, Value *args, int arg_count, Value *out,
                 const char **error_message);

#endif