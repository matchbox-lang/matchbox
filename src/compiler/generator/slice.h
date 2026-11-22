#ifndef GENERATOR_SLICE_H
#define GENERATOR_SLICE_H

#include "compiler/generator/operand.h"

Operand compile_slice(Compiler* compiler, ASTNode* ast);
Operand compile_slice_length(Compiler* compiler, ASTNode* ast, bool discard);

#endif
