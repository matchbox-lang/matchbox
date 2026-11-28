#ifndef GENERATOR_INDEX_H
#define GENERATOR_INDEX_H

#include "compiler/generator/operand.h"

Operand compile_index(Compiler* compiler, ASTNode* ast);
Operand compile_index_address(Compiler* compiler, ASTNode* ast);
void compile_index_assignment(Compiler* compiler, ASTNode* ast);
void compile_index_panic(Compiler* compiler);
void scale_index(Compiler* compiler, Operand index, size_t stride);

#endif
