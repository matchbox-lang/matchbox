#ifndef CONSTANT_H
#define CONSTANT_H

#include "compiler/ast.h"

typedef struct ConstantValue
{
    uint64_t integer;
    double floating;
} ConstantValue;

bool evaluate_integer_constant(const ASTNode* ast, uint64_t* value);
bool validate_string_slice_constant(const ASTNode* ast);
bool evaluate_constant(const ASTNode* ast, ConstantValue* value);
ASTNode* create_constant_node(ConstantValue value, Type type, Token token);

#endif
