#ifndef BUILTIN_CONSTANT_H
#define BUILTIN_CONSTANT_H

#include "compiler/ast.h"

ASTNode* resolve_builtin_constant(DataType type, Token name);

#endif
