#ifndef GENERATOR_VARIABLE_H
#define GENERATOR_VARIABLE_H

#include "compiler/generator/operand.h"

int get_local_position(Compiler* compiler, ASTNode* ast);
Operand reference_variable(Compiler* compiler, ASTNode* ast);
Operand load_variable_value(Compiler* compiler, ASTNode* symbol);
bool is_direct_variable(Compiler* compiler, ASTNode* ast);
Operand compile_variable(Compiler* compiler, ASTNode* ast);
void compile_assignment(Compiler* compiler, ASTNode* ast);
void compile_variable_definition(Compiler* compiler, ASTNode* ast);

#endif
