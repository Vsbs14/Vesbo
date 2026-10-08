#ifndef VESBO_INTERPRETER_H
#define VESBO_INTERPRETER_H

#include "ast.h"
#include "environment.h"

// Runs a fully-parsed program. Looks for and calls a "main" function
// if one is declared, after registering all top-level function decls.
void interpret_program(StmtList program);

// Runs statements in REPL mode against an interactive environment,
// auto-printing expression results.
void interpret_repl_program(StmtList program, Environment *env);

// Cleans up function definitions stored in the interpreter.
void interpret_cleanup(void);

#endif