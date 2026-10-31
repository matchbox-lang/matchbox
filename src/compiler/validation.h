#ifndef VALIDATION_H
#define VALIDATION_H

#include "compiler/ast.h"
#include <stddef.h>

void validateConstantExpressions(const ASTNode* ast, size_t start);

#endif
