#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ast.h"
#include <stddef.h>

void optimize(AST* ast, size_t start);

#endif
