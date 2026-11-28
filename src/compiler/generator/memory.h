#ifndef GENERATOR_MEMORY_H
#define GENERATOR_MEMORY_H

#include "compiler/generator/operand.h"
#include "vm/opcode.h"

void advance_offset(Compiler* compiler, Operand offset, size_t stride);
void emit_memory_access(Compiler* compiler, Opcode opcode, int value, int buffer, size_t offset);
Operand load_memory_value(Compiler* compiler, Operand buffer, Operand offset, Type type);
void store_memory_value(Compiler* compiler, Operand buffer, Operand value, size_t offset, Type type);
Opcode get_memory_load_opcode(Type type);
Opcode get_memory_store_opcode(Type type);

#endif
