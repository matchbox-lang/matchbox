#ifndef BUILTIN_H
#define BUILTIN_H

#include "function_object.h"
#include "token.h"
#include "value.h"

#define BUILTINS_MAX 95
#define BUILTIN_PARAMS_MAX 4
#define BUILTIN_VARIADIC -1
#define BUILTIN_TYPED_VARIADIC -2

typedef struct Builtin
{
    const char* name;
    FunctionEntry entry;
    int paramCount;
    TokenType params[BUILTIN_PARAMS_MAX];
    TokenType returnTypeId;
} Builtin;

bool isBuiltinName(const char* name);
Builtin* resolveBuiltin(const char* name, TokenType* argumentTypes, size_t argumentCount);

#endif
