#ifndef BUILTIN_H
#define BUILTIN_H

#include "function_object.h"
#include "token.h"
#include "value.h"

#define BUILTINS_MAX 8

typedef enum BuiltinId
{
    BUILTIN_EXIT,
    BUILTIN_PRINT,
    BUILTIN_CLAMP,
    BUILTIN_ABS,
    BUILTIN_MIN,
    BUILTIN_MAX,
    BUILTIN_POW,
    BUILTIN_BYTEORDER
} BuiltinId;

typedef struct Builtin
{
    const char* name;
    BuiltinId id;
    FunctionEntry entry;
    int paramCount;
    TokenType params[4];
    TokenType typeId;
} Builtin;

extern Builtin builtins[BUILTINS_MAX];

Builtin* getBuiltinByName(const char* name);
void builtinExit(VM* vm, FunctionObject* function, Value* frame);
void builtinPrint(VM* vm, FunctionObject* function, Value* frame);
void builtinClamp(VM* vm, FunctionObject* function, Value* frame);
void builtinAbs(VM* vm, FunctionObject* function, Value* frame);
void builtinMin(VM* vm, FunctionObject* function, Value* frame);
void builtinMax(VM* vm, FunctionObject* function, Value* frame);
void builtinPow(VM* vm, FunctionObject* function, Value* frame);
void builtinByteorder(VM* vm, FunctionObject* function, Value* frame);

#endif
