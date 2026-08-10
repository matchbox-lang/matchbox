#ifndef BUILTIN_H
#define BUILTIN_H

#include "function_object.h"
#include "token.h"
#include "value.h"

#define BUILTINS_MAX 19
#define BUILTIN_PARAMS_MAX 4

typedef struct Builtin
{
    const char* name;
    FunctionEntry entry;
    int paramCount;
    TokenType params[BUILTIN_PARAMS_MAX];
    TokenType returnTypeId;
} Builtin;

extern Builtin builtins[BUILTINS_MAX];

Builtin* resolveBuiltin(const char* name, TokenType* argumentTypes, size_t argumentCount);
bool isBuiltinName(const char* name);
void builtinExit(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintI32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintI64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintU32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintU64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintF32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintF64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintBool(VM* vm, FunctionObject* function, Value* frame);
void builtinClamp(VM* vm, FunctionObject* function, Value* frame);
void builtinClampI64(VM* vm, FunctionObject* function, Value* frame);
void builtinAbs(VM* vm, FunctionObject* function, Value* frame);
void builtinAbsI64(VM* vm, FunctionObject* function, Value* frame);
void builtinMin(VM* vm, FunctionObject* function, Value* frame);
void builtinMinI64(VM* vm, FunctionObject* function, Value* frame);
void builtinMax(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxI64(VM* vm, FunctionObject* function, Value* frame);
void builtinPow(VM* vm, FunctionObject* function, Value* frame);
void builtinPowI64(VM* vm, FunctionObject* function, Value* frame);
void builtinByteorder(VM* vm, FunctionObject* function, Value* frame);

#endif
