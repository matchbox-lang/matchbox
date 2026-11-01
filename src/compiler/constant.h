#ifndef CONSTANT_H
#define CONSTANT_H

#include "compiler/ast.h"

typedef struct ConstantValue
{
    uint64_t integer;
    double floating;
} ConstantValue;

bool evaluate_constant(const ASTNode* ast, ConstantValue* value);
ASTNode* create_constant_node(ConstantValue value, DataType type, Token token);

#endif
