#ifndef BUILTIN_IO_H
#define BUILTIN_IO_H

#include "function_object.h"
#include "value.h"

void builtinPrint(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintBool(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintChar(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintF32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintF64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintI32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintI64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintString(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintU32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintU64(VM* vm, FunctionObject* function, Value* frame);

#endif
