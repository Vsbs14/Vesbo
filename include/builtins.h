#ifndef VESBO_BUILTINS_H
#define VESBO_BUILTINS_H

#include "value.h"

// Returns 1 on success, 0 when name is not a built-in, and -1 when the
// built-in reports a runtime error. On failure, *error_message is set.
int call_builtin(const char *name, Value *args, int arg_count, Value *out,
				 const char **error_message);

#endif