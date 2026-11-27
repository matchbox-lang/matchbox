#ifndef GENERATOR_TUPLE_H
#define GENERATOR_TUPLE_H

#include "compiler/generator/operand.h"

Operand compile_tuple(Compiler* compiler, ASTNode* ast);
Operand compile_tuple_index(Compiler* compiler, ASTNode* ast, bool reference);
Operand compile_tuple_address(Compiler* compiler, ASTNode* ast);
void compile_tuple_index_assignment(Compiler* compiler, ASTNode* ast);
void store_tuple_slots(Compiler* compiler, Operand buffer, Operand value, size_t offset);
Operand load_tuple_slots(Compiler* compiler, Operand buffer, size_t offset, Type type);
Operand emit_tuple_copy(Compiler* compiler, const Builtin* builtin, Type type, Operand source);

#endif
