#ifndef FUNCTION_OBJECT_H
#define FUNCTION_OBJECT_H

#include "object.h"
#include "chunk.h"
#include "stringobject.h"

#define AS_FUNCTION_OBJECT(value) ((FunctionObject*)AS_OBJECT(value))

typedef struct FunctionObject
{
    Object obj;
    int paramCount;
    int localCount;
    int maxStackCount;
    Chunk chunk;
} FunctionObject;

FunctionObject* createFunctionObject();
void freeFunctionObject(FunctionObject* function);

#endif
