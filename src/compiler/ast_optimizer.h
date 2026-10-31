#ifndef AST_OPTIMIZER_H
#define AST_OPTIMIZER_H

#include "compiler/ast.h"
#include <stddef.h>

void optimizeAST(ASTNode* ast, size_t start);

void removeDiscardedReads(Vector* nodes, size_t start);
void markReachableFunctions(Vector* nodes);

#endif
