#ifndef GENERATOR_ARRAY_H
#define GENERATOR_ARRAY_H

#include "compiler/generator/operand.h"

Operand compile_array(Compiler* compiler, ASTNode* ast);
Operand compile_array_binary(Compiler* compiler, ASTNode* ast);
void compile_array_compound_assignment(Compiler* compiler, ASTNode* ast);
Operand compile_array_copy(Compiler* compiler, ASTNode* ast, bool discard);
Operand compile_array_length(Compiler* compiler, ASTNode* ast, bool discard);
Operand compile_array_prefix(Compiler* compiler, ASTNode* ast);
Operand emit_array_copy(Compiler* compiler, const Builtin* definition, Type type, Operand source, bool discard);
void store_array_value(Compiler* compiler, Operand buffer, Operand value, size_t offset, Type type);

#endif
