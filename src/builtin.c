#include "builtin.h"
#include "token.h"
#include "value.h"
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Builtin builtins[BUILTINS_MAX] = {
    {"exit",        builtinExit,        0,  {},                                 TOKEN_VOID},
    {"print",       builtinPrintI32,    1,  {TOKEN_I32},                        TOKEN_VOID},
    {"print",       builtinPrintI64,    1,  {TOKEN_I64},                        TOKEN_VOID},
    {"clamp",       builtinClamp,       3,  {TOKEN_I32, TOKEN_I32, TOKEN_I32},  TOKEN_I32},
    {"clamp",       builtinClampI64,    3,  {TOKEN_I64, TOKEN_I64, TOKEN_I64},  TOKEN_I64},
    {"abs",         builtinAbs,         1,  {TOKEN_I32},                        TOKEN_I32},
    {"abs",         builtinAbsI64,      1,  {TOKEN_I64},                        TOKEN_I64},
    {"min",         builtinMin,         2,  {TOKEN_I32, TOKEN_I32},             TOKEN_I32},
    {"min",         builtinMinI64,      2,  {TOKEN_I64, TOKEN_I64},             TOKEN_I64},
    {"max",         builtinMax,         2,  {TOKEN_I32, TOKEN_I32},             TOKEN_I32},
    {"max",         builtinMaxI64,      2,  {TOKEN_I64, TOKEN_I64},             TOKEN_I64},
    {"pow",         builtinPow,         2,  {TOKEN_I32, TOKEN_I32},             TOKEN_I32},
    {"pow",         builtinPowI64,      2,  {TOKEN_I64, TOKEN_I64},             TOKEN_I64},
    {"byteorder",   builtinByteorder,   0,  {},                                 TOKEN_I32}
};

static int64_t readI64(Value* frame, size_t position)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = AS_U32(frame[position]) | ((uint64_t)AS_U32(frame[position + 1]) << 32);

    return (int64_t)bits;
#else
    return AS_SIGNED(frame[position]);
#endif
}

static void writeI64(Value* frame, int64_t value)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = (uint64_t)value;
    
    frame[-2] = U32_VALUE(bits);
    frame[-1] = U32_VALUE(bits >> 32);
#else
    frame[-2] = SIGNED_VALUE(value);
#endif
}

static bool builtinArgumentMatches(Builtin* builtin, TokenType* argumentTypes, size_t position)
{
    TokenType argumentType = argumentTypes[position];
    TokenType parameterType = builtin->params[position];

    return argumentType == TOKEN_UNKNOWN
        || argumentType == parameterType
        || canImplicitlyWidenInteger(argumentType, parameterType);
}

static bool builtinMatches(Builtin* builtin, TokenType* argumentTypes, size_t argumentCount)
{
    if ((size_t)builtin->paramCount != argumentCount) {
        return false;
    }

    for (size_t i = 0; i < argumentCount; i++) {
        if (!builtinArgumentMatches(builtin, argumentTypes, i)) {
            return false;
        }
    }

    return true;
}

static bool builtinCallMatches(
    Builtin* builtin, const char* name, TokenType* argumentTypes, size_t argumentCount)
{
    return strcmp(name, builtin->name) == 0
        && builtinMatches(builtin, argumentTypes, argumentCount);
}

Builtin* resolveBuiltin(const char* name, TokenType* argumentTypes, size_t argumentCount)
{
    for (int i = 0; i < BUILTINS_MAX; i++) {
        Builtin* builtin = &builtins[i];

        if (builtinCallMatches(builtin, name, argumentTypes, argumentCount)) {
            return builtin;
        }
    }

    return NULL;
}

void builtinExit(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;
    (void)frame;

    exit(0);
}

void builtinPrintI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_I32(frame[0]);
    
    printf("%" PRId32 "\n", n);
}

void builtinPrintI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t n = readI64(frame, 0);

    printf("%" PRId64 "\n", n);
}

void builtinClamp(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t num = AS_SIGNED(frame[0]);
    int32_t min = AS_SIGNED(frame[1]);
    int32_t max = AS_SIGNED(frame[2]);

    if (num < min) {
        num = min;
    } else if (num > max) {
        num = max;
    }

    frame[-2] = SIGNED_VALUE(num);
}

void builtinAbs(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_SIGNED(frame[0]);
    int32_t x = n < 0 ? -n : n;
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtinClampI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t num = readI64(frame, 0);
    int64_t min = readI64(frame, 2);
    int64_t max = readI64(frame, 4);

    if (num < min) {
        num = min;
    } else if (num > max) {
        num = max;
    }

    writeI64(frame, num);
}

void builtinAbsI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t n = readI64(frame, 0);
    int64_t x = n < 0 ? -n : n;

    writeI64(frame, x);
}

void builtinMin(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = a < b ? a : b;
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtinMinI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = readI64(frame, 0);
    int64_t b = readI64(frame, 2);
    int64_t x = a < b ? a : b;

    writeI64(frame, x);
}

void builtinMax(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = a > b ? a : b;
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtinMaxI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = readI64(frame, 0);
    int64_t b = readI64(frame, 2);
    int64_t x = a > b ? a : b;

    writeI64(frame, x);
}

void builtinPow(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = pow(a, b);

    frame[-2] = SIGNED_VALUE(x);
}

void builtinPowI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = readI64(frame, 0);
    int64_t b = readI64(frame, 2);
    int64_t x = pow(a, b);

    writeI64(frame, x);
}

void builtinByteorder(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t i = 1;
    char* c = (char*)&i;
    
    frame[-2] = SIGNED_VALUE(*c == 0);
}
