#ifndef AST_OPTIMIZER_H
#define AST_OPTIMIZER_H

#include "compiler/ast.h"
#include <stddef.h>

void optimize_ast(ASTNode* ast, size_t start);
void remove_discarded_reads(Vector* nodes, size_t start);
void mark_reachable_functions(Vector* nodes);

#endif
