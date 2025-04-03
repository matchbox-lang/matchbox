#include "functionobject.h"
#include "object.h"
#include <stdio.h>

FunctionObject* createFunctionObject()
{
    FunctionObject* function = ALLOCATE_OBJECT(FunctionObject, OBJ_FUNCTION);
    function->paramCount = 0;
    function->localCount = 0;
    function->maxStackCount = 0;
    initCodeObject(&function->code);

    return function;
}

void freeFunctionObject(FunctionObject* function)
{
    freeCodeObject(&function->code);
    free(function);
}
