#ifndef VESBO_H
#define VESBO_H

#include "value.h"
#include "bytecode.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VesboVM VesboVM;

/* Signature for host C functions registered into Vesbo */
typedef Value (*VesboNativeFn)(VesboVM *vm, int argc, Value *argv);

/* Create a new isolated Vesbo runtime instance */
VesboVM *vesbo_new(void);

/* Free a Vesbo runtime instance and its associated resources */
void vesbo_free(VesboVM *vm);

/* Load and execute Vesbo source code or a .vsb file path.
 * Returns 0 on success, non-zero on error. */
int vesbo_load(VesboVM *vm, const char *source_or_path);

/* Explicitly load and execute a .vsb file from disk.
 * Returns 0 on success, non-zero on error. */
int vesbo_load_file(VesboVM *vm, const char *filepath);

/* Explicitly load and execute a Vesbo source string with a virtual filename.
 * Returns 0 on success, non-zero on error. */
int vesbo_load_string(VesboVM *vm, const char *source, const char *filename);

/* Register a C callback as a callable built-in function in Vesbo */
void vesbo_register_native(VesboVM *vm, const char *name, VesboNativeFn fn);

/* Create a new coroutine instance from a declared Vesbo function name */
Value vesbo_coroutine(VesboVM *vm, const char *func_name, int argc, Value *argv);

/* Resume a coroutine (or coroutine function name).
 * Returns 1 when the coroutine yields (still suspended),
 *         0 when the coroutine finishes (returns),
 *        -1 on runtime error.
 * If out is non-NULL, receives the yielded or returned Value. */
int vesbo_resume(VesboVM *vm, Value coroutine, Value *out);

/* Resume a coroutine while sending a value to the suspended yield expression */
int vesbo_resume_with(VesboVM *vm, Value coroutine, Value send_val, Value *out);

/* Call a Vesbo function by name from C. Returns 0 on success, -1 on error. */
int vesbo_call(VesboVM *vm, const char *func_name, int argc, Value *argv, Value *out);

/* Read or write a global variable in the VM */
Value vesbo_get_global(VesboVM *vm, const char *name);
void vesbo_set_global(VesboVM *vm, const char *name, Value val);

/* Raise a runtime error from inside a VesboNativeFn callback */
void vesbo_error(VesboVM *vm, const char *message);

/* Retrieve the last error message or formatted stack trace */
const char *vesbo_last_error(VesboVM *vm);
const char *vesbo_last_stack_trace(VesboVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* VESBO_H */
