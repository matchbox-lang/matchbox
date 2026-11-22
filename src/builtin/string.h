#ifndef BUILTIN_STRING_H
#define BUILTIN_STRING_H

#include "runtime/function_object.h"
#include "runtime/value.h"

void builtin_copy_string(VM* vm, const FunctionObject* function, Value* frame);
void builtin_string(VM* vm, const FunctionObject* function, Value* frame);
void builtin_string_slices_equal(VM* vm, const FunctionObject* function, Value* frame);
void builtin_validate_string_slice(VM* vm, const FunctionObject* function, Value* frame);

#endif
