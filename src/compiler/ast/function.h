#ifndef AST_FUNCTION_ARRAY_EFFECTS_H
#define AST_FUNCTION_ARRAY_EFFECTS_H

#include "common/vector.h"

struct FunctionArrayEffects
{
    Vector moves;
    Vector initializations;
    Vector possible_initializations;
    Vector uninitialized_returns;
};

#endif
