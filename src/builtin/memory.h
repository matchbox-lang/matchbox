#ifndef BUILTIN_MEMORY_H
#define BUILTIN_MEMORY_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtin_create_array(VM* vm, const FunctionObject* function, Value* frame);
void builtin_copy_array(VM* vm, const FunctionObject* function, Value* frame);

#endif
