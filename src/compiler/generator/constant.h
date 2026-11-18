#ifndef GENERATOR_CONSTANT_H
#define GENERATOR_CONSTANT_H

#include "compiler/generator/operand.h"
#include <stddef.h>
#include <stdint.h>

size_t make_i64_constant(Compiler* compiler, uint64_t value);
Operand compile_integer_value(Compiler* compiler, uint64_t value, Type type);
Operand compile_float_value(Compiler* compiler, double value, Type type);
Operand compile_float(Compiler* compiler, ASTNode* ast);
Operand compile_string(Compiler* compiler, ASTNode* ast);
Operand compile_character(Compiler* compiler, ASTNode* ast);
Operand compile_boolean(Compiler* compiler, ASTNode* ast);
void evaluate_constant_expression(Compiler* compiler, ASTNode* ast);
Operand compile_constant(Compiler* compiler, ASTNode* ast);
Operand compile_usize_literal(Compiler* compiler, size_t value);

#endif
