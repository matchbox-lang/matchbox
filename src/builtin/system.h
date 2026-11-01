#ifndef BUILTIN_SYSTEM_H
#define BUILTIN_SYSTEM_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtin_assert(VM* vm, const FunctionObject* function, Value* frame);
void builtin_assert_message(VM* vm, const FunctionObject* function, Value* frame);
void builtin_byteorder(VM* vm, const FunctionObject* function, Value* frame);
void builtin_exit(VM* vm, const FunctionObject* function, Value* frame);
void builtin_exit_status(VM* vm, const FunctionObject* function, Value* frame);
void builtin_panic(VM* vm, const FunctionObject* function, Value* frame);

#endif
