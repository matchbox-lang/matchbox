#ifndef BUILTIN_SYSTEM_H
#define BUILTIN_SYSTEM_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtinAssert(VM* vm, const FunctionObject* function, Value* frame);
void builtinAssertMessage(VM* vm, const FunctionObject* function, Value* frame);
void builtinByteorder(VM* vm, const FunctionObject* function, Value* frame);
void builtinExit(VM* vm, const FunctionObject* function, Value* frame);
void builtinExitStatus(VM* vm, const FunctionObject* function, Value* frame);
void builtinPanic(VM* vm, const FunctionObject* function, Value* frame);

#endif
