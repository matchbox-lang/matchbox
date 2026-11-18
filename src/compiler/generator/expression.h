#ifndef GENERATOR_EXPRESSION_H
#define GENERATOR_EXPRESSION_H

#include "compiler/generator/operand.h"
#include "compiler/token.h"
#include "vm/opcode.h"

typedef enum IntegerOperation
{
    IntAdd,
    IntSub,
    IntMul,
    IntDiv,
    IntRem,
    IntBand,
    IntBor,
    IntBxor,
    IntBnot,
    IntLsl,
    IntLsr,
    IntNeg
} IntegerOperation;

Opcode get_width_opcode(Type type, Opcode opcode64, Opcode opcode32, Opcode opcode16, Opcode opcode8);
Opcode get_integer_opcode(Type type, IntegerOperation operation);
Operand emit_binary_operands(Compiler* compiler, IntegerOperation operation, Type type, Operand left, Operand right);
Operand emit_immediate_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right);
Operand emit_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right);
Opcode get_float_opcode(Type type, TokenType operator_type);
Operand emit_float_operands(Compiler* compiler, Opcode opcode, Type type, Operand left, Operand right);
Opcode get_three_way_opcode(Type type, TokenType operator_type);
Operand emit_numeric_comparison(Compiler* compiler, TokenType operator_type, Type type, Operand left, Operand right);
Operand normalize_integer_operand(Compiler* compiler, Operand operand, Type type);
Operand compile_comparison_operand(Compiler* compiler, ASTNode* ast, Type type);
Operand emit_binary_operator(Compiler* compiler, TokenType operator_type, Type type, Operand left, Operand right);
Operand compile_binary(Compiler* compiler, ASTNode* ast);
Operand emit_prefix_operand(Compiler* compiler, TokenType operator_type, Type type, Operand operand);
Operand compile_prefix(Compiler* compiler, ASTNode* ast);

#endif
