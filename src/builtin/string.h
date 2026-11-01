#ifndef BUILTIN_STRING_H
#define BUILTIN_STRING_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtin_string(VM* vm, const FunctionObject* function, Value* frame);

#endif
