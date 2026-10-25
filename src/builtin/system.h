#ifndef BUILTIN_SYSTEM_H
#define BUILTIN_SYSTEM_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtinByteorder(VM* vm, FunctionObject* function, Value* frame);
void builtinExit(VM* vm, FunctionObject* function, Value* frame);
void builtinExitStatus(VM* vm, FunctionObject* function, Value* frame);

#endif
