#ifndef CONSTANT_H
#define CONSTANT_H

#include "compiler/ast.h"

typedef struct ConstantValue
{
    uint64_t integer;
    double floating;
} ConstantValue;

bool evaluateConstant(ASTNode* ast, ConstantValue* value);
ASTNode* createConstantNode(ConstantValue value, DataType type, Token token);

#endif
