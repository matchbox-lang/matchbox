#ifndef GENERATOR_CONTROL_FLOW_H
#define GENERATOR_CONTROL_FLOW_H

#include "compiler/generator/operand.h"

Operand compile_logical_binary(Compiler* compiler, ASTNode* ast);
Operand compile_conditional_branch(Compiler* compiler, ASTNode* branch, Type type, int destination, bool reference);
Operand allocate_branch_result(Compiler* compiler, Type type);
Operand compile_conditional(Compiler* compiler, ASTNode* ast, bool reference);
void compile_while(Compiler* compiler, ASTNode* ast);

#endif
