#ifndef VESBO_VM_H
#define VESBO_VM_H

#include "bytecode.h"
#include "value.h"

#define VM_STACK_SIZE 256
#define VM_CALL_STACK_SIZE 64
#define VM_TRY_STACK_SIZE 64

/* Call frame for function calls */
typedef struct {
    uint8_t *return_ip;     /* Return instruction pointer */
    int local_var_base;     /* Base index (into vm->stack) for this frame's locals */
    int try_sp;              /* Saved try_sp, so a returning function drops its own handlers */
} CallFrame;

/* A single active try/catch handler */
typedef struct {
    uint8_t *catch_ip;      /* Where to jump on error */
    int sp;                 /* Stack pointer to restore before jumping */
    int call_sp;             /* Call stack depth at the time the try began */
} TryFrame;

/* Global variable slot: name + value, looked up by name */
typedef struct {
    char *name;
    Value value;
} GlobalVar;

/* Virtual machine for executing bytecode */
typedef struct {
    Chunk *chunk;
    uint8_t *ip;                    /* Instruction pointer */
    Value *stack;                   /* Value stack */
    int sp;                         /* Stack pointer */
    CallFrame *call_stack;          /* Function call stack */
    int call_sp;                    /* Call stack pointer */
    GlobalVar *globals;              /* Global variables, looked up by name */
    int global_cap;
    int global_count;
    TryFrame *try_stack;             /* Active try/catch handlers */
    int try_sp;
    int had_error;                   /* Set when a runtime error is raised */
    char error_message[256];         /* Message for the most recent error */
} VM;

/* Create and initialize a VM */
VM *vm_new(Chunk *chunk);

/* Free the VM */
void vm_free(VM *vm);

/* Execute bytecode */
int vm_execute(VM *vm);

/* Push/pop stack operations */
void vm_push(VM *vm, Value val);
Value vm_pop(VM *vm);
Value vm_peek(VM *vm);

#endif /* VESBO_VM_H */
