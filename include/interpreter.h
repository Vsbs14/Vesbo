#ifndef VESBO_INTERPRETER_H
#define VESBO_INTERPRETER_H

#include "ast.h"

// Runs a fully-parsed program. Looks for and calls a "main" function
// if one is declared, after registering all top-level function decls.
void interpret_program(StmtList program);

#endif