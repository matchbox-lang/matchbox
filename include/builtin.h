#ifndef BUILTIN_H
#define BUILTIN_H

#include "function_object.h"
#include "token.h"
#include "value.h"

#define BUILTINS_MAX 95
#define BUILTIN_PARAMS_MAX 4
#define BUILTIN_VARIADIC -1
#define BUILTIN_TYPED_VARIADIC -2

typedef struct Builtin
{
    const char* name;
    FunctionEntry entry;
    int paramCount;
    TokenType params[BUILTIN_PARAMS_MAX];
    TokenType returnTypeId;
} Builtin;

Builtin* resolveBuiltin(const char* name, TokenType* argumentTypes, size_t argumentCount);
bool isBuiltinName(const char* name);
void builtinByteorder(VM* vm, FunctionObject* function, Value* frame);
void builtinExit(VM* vm, FunctionObject* function, Value* frame);
void builtinPrint(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintI32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintI64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintU32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintU64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintF32(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintF64(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintString(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintChar(VM* vm, FunctionObject* function, Value* frame);
void builtinPrintBool(VM* vm, FunctionObject* function, Value* frame);
void builtinClampI32(VM* vm, FunctionObject* function, Value* frame);
void builtinClampI64(VM* vm, FunctionObject* function, Value* frame);
void builtinClampF32(VM* vm, FunctionObject* function, Value* frame);
void builtinClampF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAbsI32(VM* vm, FunctionObject* function, Value* frame);
void builtinAbsI64(VM* vm, FunctionObject* function, Value* frame);
void builtinAbsF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAbsF64(VM* vm, FunctionObject* function, Value* frame);
void builtinMinI32(VM* vm, FunctionObject* function, Value* frame);
void builtinMinI64(VM* vm, FunctionObject* function, Value* frame);
void builtinMinF32(VM* vm, FunctionObject* function, Value* frame);
void builtinMinF64(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxI32(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxI64(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxF32(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxF64(VM* vm, FunctionObject* function, Value* frame);
void builtinPowI32(VM* vm, FunctionObject* function, Value* frame);
void builtinPowI64(VM* vm, FunctionObject* function, Value* frame);
void builtinPowF32(VM* vm, FunctionObject* function, Value* frame);
void builtinPowF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAcosF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAcosF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAcoshF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAcoshF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAsinF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAsinF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAsinhF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAsinhF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAtanF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAtanF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAtan2F32(VM* vm, FunctionObject* function, Value* frame);
void builtinAtan2F64(VM* vm, FunctionObject* function, Value* frame);
void builtinAtanhF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAtanhF64(VM* vm, FunctionObject* function, Value* frame);
void builtinCbrtF32(VM* vm, FunctionObject* function, Value* frame);
void builtinCbrtF64(VM* vm, FunctionObject* function, Value* frame);
void builtinCeilF32(VM* vm, FunctionObject* function, Value* frame);
void builtinCeilF64(VM* vm, FunctionObject* function, Value* frame);
void builtinCosF32(VM* vm, FunctionObject* function, Value* frame);
void builtinCosF64(VM* vm, FunctionObject* function, Value* frame);
void builtinCoshF32(VM* vm, FunctionObject* function, Value* frame);
void builtinCoshF64(VM* vm, FunctionObject* function, Value* frame);
void builtinDegF32(VM* vm, FunctionObject* function, Value* frame);
void builtinDegF64(VM* vm, FunctionObject* function, Value* frame);
void builtinExpF32(VM* vm, FunctionObject* function, Value* frame);
void builtinExpF64(VM* vm, FunctionObject* function, Value* frame);
void builtinFloorF32(VM* vm, FunctionObject* function, Value* frame);
void builtinFloorF64(VM* vm, FunctionObject* function, Value* frame);
void builtinHypotF32(VM* vm, FunctionObject* function, Value* frame);
void builtinHypotF64(VM* vm, FunctionObject* function, Value* frame);
void builtinLdexpF32(VM* vm, FunctionObject* function, Value* frame);
void builtinLdexpF64(VM* vm, FunctionObject* function, Value* frame);
void builtinLogF32(VM* vm, FunctionObject* function, Value* frame);
void builtinLogF64(VM* vm, FunctionObject* function, Value* frame);
void builtinLog10F32(VM* vm, FunctionObject* function, Value* frame);
void builtinLog10F64(VM* vm, FunctionObject* function, Value* frame);
void builtinLog2F32(VM* vm, FunctionObject* function, Value* frame);
void builtinLog2F64(VM* vm, FunctionObject* function, Value* frame);
void builtinRadF32(VM* vm, FunctionObject* function, Value* frame);
void builtinRadF64(VM* vm, FunctionObject* function, Value* frame);
void builtinSinF32(VM* vm, FunctionObject* function, Value* frame);
void builtinSinF64(VM* vm, FunctionObject* function, Value* frame);
void builtinSinhF32(VM* vm, FunctionObject* function, Value* frame);
void builtinSinhF64(VM* vm, FunctionObject* function, Value* frame);
void builtinSqrtF32(VM* vm, FunctionObject* function, Value* frame);
void builtinSqrtF64(VM* vm, FunctionObject* function, Value* frame);
void builtinTanF32(VM* vm, FunctionObject* function, Value* frame);
void builtinTanF64(VM* vm, FunctionObject* function, Value* frame);
void builtinTanhF32(VM* vm, FunctionObject* function, Value* frame);
void builtinTanhF64(VM* vm, FunctionObject* function, Value* frame);
void builtinMinU32(VM* vm, FunctionObject* function, Value* frame);
void builtinMinU64(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxU32(VM* vm, FunctionObject* function, Value* frame);
void builtinMaxU64(VM* vm, FunctionObject* function, Value* frame);
void builtinSumI32(VM* vm, FunctionObject* function, Value* frame);
void builtinSumI64(VM* vm, FunctionObject* function, Value* frame);
void builtinSumF32(VM* vm, FunctionObject* function, Value* frame);
void builtinSumF64(VM* vm, FunctionObject* function, Value* frame);
void builtinAvgF32(VM* vm, FunctionObject* function, Value* frame);
void builtinAvgF64(VM* vm, FunctionObject* function, Value* frame);
void builtinString(VM* vm, FunctionObject* function, Value* frame);

#endif
