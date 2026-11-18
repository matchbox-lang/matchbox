#ifndef GENERATOR_CONVERSION_H
#define GENERATOR_CONVERSION_H

#include "compiler/generator/operand.h"

Operand convert_operand(Compiler* compiler, Operand operand, Type source, Type destination);
Operand compile_converted_expression(Compiler* compiler, ASTNode* ast, Type destination, bool reference);
Operand compile_explicit_conversion(Compiler* compiler, ASTNode* ast);

#endif
