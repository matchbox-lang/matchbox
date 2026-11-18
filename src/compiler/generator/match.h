#ifndef GENERATOR_MATCH_H
#define GENERATOR_MATCH_H

#include "compiler/generator/operand.h"

Operand compile_match(Compiler* compiler, ASTNode* ast, bool reference);

#endif
