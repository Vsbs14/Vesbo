#ifndef VESBO_VM_H
#define VESBO_VM_H

#include "bytecode.h"
#include "value.h"

#define VM_INITIAL_STACK_SIZE 256
#define VM_MAX_STACK_SIZE     1048576
#define VM_INITIAL_CALL_SIZE  64
#define VM_MAX_CALL_SIZE      16384
#define VM_INITIAL_TRY_SIZE   16
#define VM_MAX_TRY_SIZE       4096

/* Call frame for function calls */
typedef struct {
    uint8_t *return_ip;     /* Return instruction pointer */
    uint8_t *call_site_ip;  /* Instruction pointer of the call site (for stack traces) */
    int func_idx;           /* Index in chunk->functions (-1 for top-level) */
    int local_var_base;     /* Base index (into vm->stack) for this frame's locals */
    int try_sp;             /* Saved try_sp, so a returning function drops its own handlers */
} CallFrame;

/* A single active try/catch handler */
typedef struct {
    uint8_t *catch_ip;      /* Where to jump on error */
    int sp;                 /* Stack pointer to restore before jumping */
    int call_sp;            /* Call stack depth at the time the try began */
} TryFrame;

/* Heap-allocated coroutine with separate value stack, frame stack, and try-stack */
struct ObjCoroutine {
    Obj obj;
    CoroState state;
    int started;
    int func_idx;
    char *func_name;
    uint8_t *ip;
    Value *stack;
    int sp;
    int stack_cap;
    CallFrame *call_stack;
    int call_sp;
    int call_cap;
    TryFrame *try_stack;
    int try_sp;
    int try_cap;
    Value *init_args;
    int init_arg_count;
    struct ObjCoroutine *caller;
};

/* Saved main execution context when a coroutine is running */
typedef struct {
    uint8_t *ip;
    Value *stack;
    int sp;
    int stack_cap;
    CallFrame *call_stack;
    int call_sp;
    int call_cap;
    TryFrame *try_stack;
    int try_sp;
    int try_cap;
} VMContext;

/* Virtual machine for executing bytecode */
typedef struct VM {
    Chunk *chunk;
    uint8_t *ip;                    /* Instruction pointer */
    Value *stack;                   /* Growable value stack */
    int sp;                         /* Stack pointer */
    int stack_cap;                  /* Current stack capacity */
    CallFrame *call_stack;          /* Growable function call stack */
    int call_sp;                    /* Call stack pointer */
    int call_cap;                   /* Current call stack capacity */
    Value *globals;                 /* Compile-time slot-indexed global values */
    uint8_t *global_defined;        /* 1 if slot has been defined */
    int global_cap;
    int global_count;
    TryFrame *try_stack;            /* Growable active try/catch handlers */
    int try_sp;
    int try_cap;
    ObjCoroutine *current_coro;     /* Currently executing coroutine (NULL if on main stack) */
    ObjCoroutine **pinned_coros;    /* Host-created coroutines rooted in this VM */
    int pinned_coro_count;
    int pinned_coro_cap;
    void *host_owner;               /* Owning VesboVM* for isolated native callbacks */
    VMContext main_ctx;             /* Saved main stack context while coroutine runs */
    int has_saved_main;
    int had_error;                  /* Set when a runtime error is raised */
    char error_message[512];        /* Message for the most recent error */
    char stack_trace[4096];         /* Formatted stack trace with file:line:col */
} VM;

/* Create and initialize a VM */
VM *vm_new(Chunk *chunk);

/* Free the VM */
void vm_free(VM *vm);

/* Execute bytecode */
int vm_execute(VM *vm);

/* Call a specific function in the VM */
int vm_call_function(VM *vm, int func_idx, int arg_count, Value *args, Value *out);

/* Resume a coroutine from C or interpreter */
/* Returns 0 when coroutine finishes (returns), 1 when it yields, -1 on error */
int vm_resume_coroutine(VM *vm, ObjCoroutine *co, int has_send_val, Value send_val, Value *out);
void vm_pin_coroutine(VM *vm, ObjCoroutine *co);

/* Global variable helpers by slot or name */
void vm_set_global_slot(VM *vm, uint32_t slot, Value val);
Value vm_get_global_slot(VM *vm, uint32_t slot, int *is_defined);
void vm_set_global_by_name(VM *vm, const char *name, Value val);
int vm_get_global_by_name(VM *vm, const char *name, Value *out);

/* Push/pop stack operations */
void vm_push(VM *vm, Value val);
Value vm_pop(VM *vm);
Value vm_peek(VM *vm);

#endif /* VESBO_VM_H */
