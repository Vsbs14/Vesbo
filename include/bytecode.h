#ifndef VESBO_BYTECODE_H
#define VESBO_BYTECODE_H

#include <stdint.h>
#include <stddef.h>
#include "value.h"

/* Bytecode opcodes */
typedef enum {
    /* Stack operations */
    OP_PUSH_NUMBER,         /* Push number constant: operand = index */
    OP_PUSH_STRING,         /* Push string constant: operand = index */
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,
    OP_PUSH_NONE,
    OP_PUSH_ARRAY,          /* Create array: operand = element count */
    OP_BUILD_MAP,           /* Create map: operand = entry count */
    
    /* Variables */
    OP_DEFINE_LOCAL,        /* Define local var: operand = var index */
    OP_GET_LOCAL,           /* Get local var: operand = var index */
    OP_SET_LOCAL,           /* Set local var: operand = var index */
    OP_GET_GLOBAL,          /* Get global var: operand = compile-time slot index */
    OP_SET_GLOBAL,          /* Set global var: operand = compile-time slot index */
    
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
    
    /* Array / Map operations */
    OP_INDEX,               /* Indexing: a[b] */
    OP_INDEX_SET,           /* Index assignment: a[b] = c */
    OP_SLICE,               /* Array slicing */
    
    /* Structs and Methods */
    OP_NEW_STRUCT,          /* Instantiate struct: operands = struct_idx, arg_count */
    OP_GET_FIELD,           /* Dot field read: operand = field name string idx */
    OP_SET_FIELD,           /* Dot field write: operand = field name string idx */
    OP_INVOKE,              /* Method call: operands = method name string idx, arg_count */

    /* Coroutines */
    OP_COROUTINE,           /* Create coroutine: operands = func_idx (or 0xFFFFFFFF if dynamic), arg_count */
    OP_YIELD,               /* Yield top of stack from current coroutine */
    OP_RESUME,              /* Resume coroutine: operand = arg_count (1 or 2) */

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
    OP_CALL,                /* Call function: operand = function index */
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
    OP_CALL_BUILTIN,        /* Call builtin/native function: operands = string name idx, arg count */
    
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
    char *file;             /* Source file path */
    int line;               /* Declaration line */
    int column;             /* Declaration column */
} FunctionDef;

/* Source location mapping for stack traces */
typedef struct {
    uint32_t code_offset;
    uint32_t file_str_idx;  /* Index in chunk->strings */
    int line;
    int column;
} BytecodeLoc;

/* Struct metadata stored in bytecode chunk */
typedef struct {
    char *name;
    int field_count;
    char **field_names;
    Value *field_defaults;
    StructDef *runtime_def;
} ChunkStructDef;

/* Bytecode chunk - represents compiled program */
typedef struct Chunk {
    uint8_t *code;          /* Bytecode instructions */
    size_t code_cap;        /* Capacity of code buffer */
    size_t code_len;        /* Current length of bytecode */
    
    double *numbers;        /* Constant numbers */
    size_t numbers_cap;
    size_t numbers_len;
    
    char **strings;         /* Constant strings */
    ObjString **string_objs;/* Cached interned ObjString pointers */
    size_t strings_cap;
    size_t strings_len;
    
    FunctionDef *functions; /* Function definitions */
    size_t functions_cap;
    size_t functions_len;

    char **global_names;    /* Compile-time slot-indexed global names */
    size_t globals_cap;
    size_t globals_len;

    ChunkStructDef *structs;/* Struct declarations */
    size_t structs_cap;
    size_t structs_len;

    BytecodeLoc *locs;      /* Instruction offset -> (file, line, col) */
    size_t locs_cap;
    size_t locs_len;

    uint32_t cur_file_idx;
    int cur_line;
    int cur_column;
    int has_cur_loc;
} Chunk;

/* Create a new chunk */
Chunk *chunk_new(void);

/* Free a chunk */
void chunk_free(Chunk *chunk);

/* Set current source location for subsequent emitted opcodes */
void chunk_set_location(Chunk *chunk, const char *file, int line, int column);

/* Lookup source location for a bytecode offset */
int chunk_get_location(Chunk *chunk, uint32_t offset, const char **out_file, int *out_line, int *out_col);

/* Lookup enclosing function for a bytecode offset */
int chunk_find_enclosing_function(Chunk *chunk, uint32_t offset);

/* Write an opcode to chunk */
void chunk_write_op(Chunk *chunk, uint8_t op);

/* Write an operand to chunk */
void chunk_write_operand(Chunk *chunk, uint32_t operand);

/* Add a number constant and return its index */
size_t chunk_add_number(Chunk *chunk, double value);

/* Add a string constant and return its index */
size_t chunk_add_string(Chunk *chunk, const char *str);

/* Resolve or add a compile-time global variable slot */
uint32_t chunk_resolve_global(Chunk *chunk, const char *name);

/* Find an existing global variable slot, or -1 */
int chunk_find_global(Chunk *chunk, const char *name);

/* Add or find struct definition in chunk */
int chunk_add_struct(Chunk *chunk, const char *name, int field_count, char **field_names, Value *field_defaults);
int chunk_find_struct(Chunk *chunk, const char *name);
StructDef *chunk_get_runtime_struct(Chunk *chunk, int struct_idx);

/* Read operand from bytecode at given offset */
uint32_t chunk_read_operand(Chunk *chunk, size_t *offset);

/* Add a function definition */
int chunk_add_function(Chunk *chunk, const char *name, uint32_t code_offset, int param_count);
int chunk_add_function_loc(Chunk *chunk, const char *name, uint32_t code_offset, int param_count,
                           const char *file, int line, int column);

/* Find a function by name, return index or -1 */
int chunk_find_function(Chunk *chunk, const char *name);

/* Patch a previously-written 4-byte operand at the given code offset */
void chunk_patch_operand(Chunk *chunk, size_t operand_offset, uint32_t value);

#endif /* VESBO_BYTECODE_H */
