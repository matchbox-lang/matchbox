#include "runtime/function_object.h"
#include "common/memory.h"
#include "runtime/code_object.h"
#include "runtime/object.h"
#include <stdlib.h>

FunctionObject* create_function_object()
{
    FunctionObject* function = ALLOCATE_OBJECT(FunctionObject, ObjFunction);
    function->type = FuncDefined;
    function->entry = enter_bytecode_function;
    function->local_count = 0;
    function->max_stack_count = 0;
    function->param_count = 0;
    function->return_count = 0;
    function->argument_formats = NULL;
    function->argument_count = 0;
    
    init_code_object(&function->code);

    return function;
}

void free_function_object(FunctionObject* function)
{
    free_code_object(&function->code);
    free_memory(function->argument_formats);
    free_memory(function);
}
