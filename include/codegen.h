#ifndef VESBO_CODEGEN_H
#define VESBO_CODEGEN_H

#include "ast.h"
#include "bytecode.h"

/* Code generator for compiling AST to bytecode */
typedef struct CodeGen CodeGen;

/* Create a new code generator */
CodeGen *codegen_new(void);

/* Free code generator */
void codegen_free(CodeGen *gen);

/* Compile statements to bytecode */
Chunk *codegen_compile(CodeGen *gen, Stmt **statements, int count);

#endif /* VESBO_CODEGEN_H */
