#ifndef BUILTIN_IO_H
#define BUILTIN_IO_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtinPrint(VM* vm, const FunctionObject* function, Value* frame);

#endif
