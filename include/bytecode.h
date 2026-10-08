#ifndef VESBO_BYTECODE_H
#define VESBO_BYTECODE_H

#include <stdint.h>
#include <stddef.h>

/* Bytecode opcodes */
typedef enum {
    /* Stack operations */
    OP_PUSH_NUMBER,         /* Push number constant: operand = index */
    OP_PUSH_STRING,         /* Push string constant: operand = index */
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,
    OP_PUSH_NONE,
    OP_PUSH_ARRAY,          /* Create array: operand = element count */
    
    /* Variables */
    OP_DEFINE_LOCAL,        /* Define local var: operand = var index */
    OP_GET_LOCAL,           /* Get local var: operand = var index */
    OP_SET_LOCAL,           /* Set local var: operand = var index */
    OP_GET_GLOBAL,          /* Get global var: operand = string index */
    OP_SET_GLOBAL,          /* Set global var: operand = string index */
    
    /* Arithmetic and comparison */
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_NEGATE,
    
    OP_EQUALS,
    OP_NOT_EQUALS,
    OP_LESS_THAN,
    OP_GREATER_THAN,
    OP_LESS_THAN_EQUAL,
    OP_GREATER_THAN_EQUAL,
    
    /* Logical operations */
    OP_AND,
    OP_OR,
    OP_NOT,
    
    /* Array operations */
    OP_INDEX,               /* Array indexing: a[b] */
    OP_INDEX_SET,           /* Array assignment: a[b] = c */
    OP_SLICE,               /* Array slicing */
    
    /* Control flow */
    OP_JUMP,                /* Unconditional jump: operand = target offset */
    OP_JUMP_IF_FALSE,       /* Jump if top of stack is false */
    OP_LOOP,                /* Loop marker for break/continue */
    OP_BREAK,
    OP_CONTINUE,

    /* Error handling */
    OP_TRY_BEGIN,           /* Push a catch handler: operand = catch target offset */
    OP_TRY_END,             /* Pop the current catch handler */
    
    /* Functions */
    OP_CALL,                /* Call function: operand = arg count */
    OP_RETURN,              /* Return from function */
    OP_DEFINE_FUNCTION,     /* Define function: operand = function index */
    
    /* Builtins */
    OP_OUTPUT,              /* Print top of stack */
    OP_INPUT,               /* Read input */
    OP_LENGTH,
    OP_LOWERCASE,
    OP_TRIM,
    OP_NUMBER_CAST,
    OP_UPPERCASE,
    OP_STRING_CAST,
    OP_PUSH_BACK,
    OP_TYPE_OF,
    OP_CALL_BUILTIN,        /* Call builtin function: operands = string name idx, arg count */
    
    /* Pop and discard */
    OP_POP,
    
    /* End of program */
    OP_HALT
} OpCode;

/* Function metadata */
typedef struct {
    char *name;             /* Function name */
    uint32_t code_offset;   /* Offset in bytecode where function starts */
    int param_count;        /* Number of parameters */
} FunctionDef;

/* Bytecode chunk - represents compiled program */
typedef struct {
    uint8_t *code;          /* Bytecode instructions */
    size_t code_cap;        /* Capacity of code buffer */
    size_t code_len;        /* Current length of bytecode */
    
    double *numbers;        /* Constant numbers */
    size_t numbers_cap;
    size_t numbers_len;
    
    char **strings;         /* Constant strings */
    size_t strings_cap;
    size_t strings_len;
    
    FunctionDef *functions; /* Function definitions */
    size_t functions_cap;
    size_t functions_len;
} Chunk;

/* Create a new chunk */
Chunk *chunk_new(void);

/* Free a chunk */
void chunk_free(Chunk *chunk);

/* Write an opcode to chunk */
void chunk_write_op(Chunk *chunk, uint8_t op);

/* Write an operand to chunk */
void chunk_write_operand(Chunk *chunk, uint32_t operand);

/* Add a number constant and return its index */
size_t chunk_add_number(Chunk *chunk, double value);

/* Add a string constant and return its index */
size_t chunk_add_string(Chunk *chunk, const char *str);

/* Read operand from bytecode at given offset */
uint32_t chunk_read_operand(Chunk *chunk, size_t *offset);

/* Add a function definition */
int chunk_add_function(Chunk *chunk, const char *name, uint32_t code_offset, int param_count);

/* Find a function by name, return index or -1 */
int chunk_find_function(Chunk *chunk, const char *name);

/* Patch a previously-written 4-byte operand at the given code offset
 * (offset points at the first byte of the operand, i.e. one past the opcode). */
void chunk_patch_operand(Chunk *chunk, size_t operand_offset, uint32_t value);

#endif /* VESBO_BYTECODE_H */
