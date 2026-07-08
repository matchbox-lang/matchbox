#ifndef BUILTIN_H
#define BUILTIN_H

#include "value.h"

#define BUILTINS_MAX 7

typedef struct Builtin
{
    char* name;
    int opcode;
    int paramCount;
    int params[4];
    int typeId;
} Builtin;

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

extern Builtin builtins[BUILTINS_MAX];

Builtin* getBuiltinByName(char* name);
Value builtinExit(Value* args);
Value builtinPrint(Value* args);
Value builtinClamp(Value* args);
Value builtinAbs(Value* args);
Value builtinMin(Value* args);
Value builtinMax(Value* args);
Value builtinByteorder(Value* args);

#endif
