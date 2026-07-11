#ifndef FUNCTION_OBJECT_H
#define FUNCTION_OBJECT_H

#include "code_object.h"

#define AS_FUNCTION_OBJECT(value) ((FunctionObject*)AS_OBJECT(value))

typedef enum FunctionType
{
    FUNCTION_BUILTIN,
    FUNCTION_DEFINED
} FunctionType;

typedef struct FunctionObject
{
    Object obj;
    FunctionType type;
    CodeObject code;
    int builtinId;
    int paramCount;
    int localCount;
    int maxStackCount;
} FunctionObject;

FunctionObject* createFunctionObject();
void freeFunctionObject(FunctionObject* function);

#endif
