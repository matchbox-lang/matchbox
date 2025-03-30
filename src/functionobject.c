#include "functionobject.h"
#include "object.h"
#include <stdio.h>

FunctionObject* createFunctionObject()
{
    FunctionObject* function = ALLOCATE_OBJECT(FunctionObject, OBJ_FUNCTION);
    function->paramCount = 0;
    function->localCount = 0;
    function->maxStackCount = 0;
    initChunk(&function->chunk);

    return function;
}

void freeFunctionObject(FunctionObject* function)
{
    freeChunk(&function->chunk);
    free(function);
}
