#ifndef BUILTIN_SYSTEM_H
#define BUILTIN_SYSTEM_H

#include "function_object.h"
#include "value.h"

void builtinByteorder(VM* vm, FunctionObject* function, Value* frame);
void builtinExit(VM* vm, FunctionObject* function, Value* frame);

#endif
