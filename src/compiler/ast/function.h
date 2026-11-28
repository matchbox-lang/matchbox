#ifndef AST_FUNCTION_ARRAY_EFFECTS_H
#define AST_FUNCTION_ARRAY_EFFECTS_H

#include "compiler/ast/ast.h"

struct FunctionArrayEffects
{
    Vector moves;
    Vector tuple_moves;
    Vector initializations;
    Vector possible_initializations;
    Vector uninitialized_returns;
};

typedef struct TupleMoveEffect
{
    ASTNode* symbol;
    size_t offset;
    bool restores;
} TupleMoveEffect;

#endif
