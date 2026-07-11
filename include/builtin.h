#ifndef BUILTIN_H
#define BUILTIN_H

#include "token.h"
#include "value.h"

#define BUILTINS_MAX 7

typedef enum BuiltinId
{
    BUILTIN_EXIT,
    BUILTIN_PRINT,
    BUILTIN_CLAMP,
    BUILTIN_ABS,
    BUILTIN_MIN,
    BUILTIN_MAX,
    BUILTIN_BYTEORDER
} BuiltinId;

typedef struct Builtin
{
    const char* name;
    BuiltinId id;
    int paramCount;
    TokenType params[4];
    TokenType typeId;
} Builtin;

extern Builtin builtins[BUILTINS_MAX];

Builtin* getBuiltinByName(const char* name);
Value builtinExit(Value* args);
Value builtinPrint(Value* args);
Value builtinClamp(Value* args);
Value builtinAbs(Value* args);
Value builtinMin(Value* args);
Value builtinMax(Value* args);
Value builtinByteorder(Value* args);

#endif
