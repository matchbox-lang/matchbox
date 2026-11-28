#ifndef GENERATOR_ARRAY_H
#define GENERATOR_ARRAY_H

#include "compiler/generator/operand.h"

Operand compile_array(Compiler* compiler, ASTNode* ast);
Operand compile_array_binary(Compiler* compiler, ASTNode* ast);
Operand emit_array_binary(Compiler* compiler, TokenType operator_type, Type left_type, Type right_type,
    Type result_type, Type operand_type, Operand left, Operand right);
void compile_array_compound_assignment(Compiler* compiler, ASTNode* ast);
Operand compile_array_copy(Compiler* compiler, ASTNode* ast, bool discard);
Operand compile_array_length(Compiler* compiler, ASTNode* ast, bool discard);
Operand compile_array_prefix(Compiler* compiler, ASTNode* ast);
Operand emit_array_prefix(Compiler* compiler, TokenType operator_type, Type type, Operand source);
Operand emit_array_copy(Compiler* compiler, const Builtin* definition, Type type, Operand source, bool discard);

#endif
