#ifndef VESBO_DISASSEMBLE_H
#define VESBO_DISASSEMBLE_H

#include "bytecode.h"

/* Disassemble an entire chunk with header and constant pool inspection */
void disassemble_chunk(Chunk *chunk, const char *name);

/* Disassemble a single instruction at offset, returning the next offset */
size_t disassemble_instruction(Chunk *chunk, size_t offset);

#endif /* VESBO_DISASSEMBLE_H */
