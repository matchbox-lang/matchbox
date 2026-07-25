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
    {"exit",        builtinExit,       0, {},                                  TOKEN_VOID},
    {"print",       builtinPrintI32,   1, {TOKEN_I32},                         TOKEN_VOID},
    {"print",       builtinPrintI64,   1, {TOKEN_I64},                         TOKEN_VOID},
    {"clamp",       builtinClamp,      3, {TOKEN_I32, TOKEN_I32, TOKEN_I32},   TOKEN_I32},
    {"abs",         builtinAbs,        1, {TOKEN_I32},                         TOKEN_I32},
    {"min",         builtinMin,        2, {TOKEN_I32, TOKEN_I32},              TOKEN_I32},
    {"max",         builtinMax,        2, {TOKEN_I32, TOKEN_I32},              TOKEN_I32},
    {"pow",         builtinPow,        2, {TOKEN_I32, TOKEN_I32},              TOKEN_I32},
    {"byteorder",   builtinByteorder,  0, {},                                  TOKEN_I32}
};

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

#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = AS_U32(frame[0]) | ((uint64_t)AS_U32(frame[1]) << 32);
    int64_t n = (int64_t)bits;
#else
    int64_t n = AS_SIGNED(frame[0]);
#endif

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

void builtinMin(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = a < b ? a : b;
    
    frame[-2] = SIGNED_VALUE(x);
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

void builtinPow(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = pow(a, b);

    frame[-2] = SIGNED_VALUE(x);
}

void builtinByteorder(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t i = 1;
    char* c = (char*)&i;
    
    frame[-2] = SIGNED_VALUE(*c == 0);
}
