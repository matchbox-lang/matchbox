#ifndef FUNCTION_OBJECT_H
#define FUNCTION_OBJECT_H

#include "codeobject.h"

#define AS_FUNCTION_OBJECT(value) ((FunctionObject*)AS_OBJECT(value))

typedef struct FunctionObject
{
    Object obj;
    CodeObject code;
    int paramCount;
    int localCount;
    int maxStackCount;
} FunctionObject;

FunctionObject* createFunctionObject();
void freeFunctionObject(FunctionObject* function);

#endif
