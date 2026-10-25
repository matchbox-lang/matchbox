#include "runtime/function_object.h"
#include "common/memory.h"
#include "runtime/code_object.h"
#include "runtime/object.h"
#include <stdlib.h>

FunctionObject* createFunctionObject()
{
    FunctionObject* function = ALLOCATE_OBJECT(FunctionObject, OBJ_FUNCTION);
    function->type = FUNCTION_DEFINED;
    function->entry = enterBytecodeFunction;
    function->localCount = 0;
    function->maxStackCount = 0;
    function->paramCount = 0;
    function->returnCount = 0;
    function->argumentTypes = NULL;
    function->argumentCount = 0;
    
    initCodeObject(&function->code);

    return function;
}

void freeFunctionObject(FunctionObject* function)
{
    freeCodeObject(&function->code);
    freeMemory(function->argumentTypes);
    freeMemory(function);
}
