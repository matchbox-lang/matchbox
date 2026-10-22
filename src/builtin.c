#include "builtin.h"
#include "float_format.h"
#include "output.h"
#include "string_object.h"
#include "string_concatenation.h"
#include "vm.h"
#include "token.h"
#include "unicode.h"
#include "value.h"
#include <util.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Builtin builtins[BUILTINS_MAX] = {
    {"byteorder",   builtinByteorder,           0,  {},                                     TOKEN_I32},
    {"exit",        builtinExit,                0,  {},                                     TOKEN_VOID},
    {"print",       builtinPrintI32,            1,  {TOKEN_I32},                            TOKEN_VOID},
    {"print",       builtinPrintI64,            1,  {TOKEN_I64},                            TOKEN_VOID},
    {"print",       builtinPrintU32,            1,  {TOKEN_U32},                            TOKEN_VOID},
    {"print",       builtinPrintU64,            1,  {TOKEN_U64},                            TOKEN_VOID},
    {"print",       builtinPrintF32,            1,  {TOKEN_F32},                            TOKEN_VOID},
    {"print",       builtinPrintF64,            1,  {TOKEN_F64},                            TOKEN_VOID},
    {"print",       builtinPrintBool,           1,  {TOKEN_BOOL},                           TOKEN_VOID},
    {"print",       builtinPrintChar,           1,  {TOKEN_CHAR},                           TOKEN_VOID},
    {"print",       builtinPrintString,         1,  {TOKEN_STRING},                         TOKEN_VOID},
    {"print",       builtinPrint,              -1,  {TOKEN_UNKNOWN},                        TOKEN_VOID},
    {"clamp",       builtinClampI32,            3,  {TOKEN_I32, TOKEN_I32, TOKEN_I32},      TOKEN_I32},
    {"clamp",       builtinClampI64,            3,  {TOKEN_I64, TOKEN_I64, TOKEN_I64},      TOKEN_I64},
    {"clamp",       builtinClampF32,            3,  {TOKEN_F32, TOKEN_F32, TOKEN_F32},      TOKEN_F32},
    {"clamp",       builtinClampF64,            3,  {TOKEN_F64, TOKEN_F64, TOKEN_F64},      TOKEN_F64},
    {"abs",         builtinAbsI32,              1,  {TOKEN_I32},                            TOKEN_I32},
    {"abs",         builtinAbsI64,              1,  {TOKEN_I64},                            TOKEN_I64},
    {"abs",         builtinAbsF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"abs",         builtinAbsF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"pow",         builtinPowI32,              2,  {TOKEN_I32, TOKEN_I32},                 TOKEN_I32},
    {"pow",         builtinPowI64,              2,  {TOKEN_I64, TOKEN_I64},                 TOKEN_I64},
    {"pow",         builtinPowF32,              2,  {TOKEN_F32, TOKEN_F32},                 TOKEN_F32},
    {"pow",         builtinPowF64,              2,  {TOKEN_F64, TOKEN_F64},                 TOKEN_F64},
    {"acos",        builtinAcosF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"acos",        builtinAcosF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"acosh",       builtinAcoshF32,            1,  {TOKEN_F32},                            TOKEN_F32},
    {"acosh",       builtinAcoshF64,            1,  {TOKEN_F64},                            TOKEN_F64},
    {"asin",        builtinAsinF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"asin",        builtinAsinF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"asinh",       builtinAsinhF32,            1,  {TOKEN_F32},                            TOKEN_F32},
    {"asinh",       builtinAsinhF64,            1,  {TOKEN_F64},                            TOKEN_F64},
    {"atan",        builtinAtanF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"atan",        builtinAtanF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"atan2",       builtinAtan2F32,            2,  {TOKEN_F32, TOKEN_F32},                 TOKEN_F32},
    {"atan2",       builtinAtan2F64,            2,  {TOKEN_F64, TOKEN_F64},                 TOKEN_F64},
    {"atanh",       builtinAtanhF32,            1,  {TOKEN_F32},                            TOKEN_F32},
    {"atanh",       builtinAtanhF64,            1,  {TOKEN_F64},                            TOKEN_F64},
    {"cbrt",        builtinCbrtF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"cbrt",        builtinCbrtF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"ceil",        builtinCeilF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"ceil",        builtinCeilF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"cos",         builtinCosF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"cos",         builtinCosF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"cosh",        builtinCoshF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"cosh",        builtinCoshF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"deg",         builtinDegF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"deg",         builtinDegF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"exp",         builtinExpF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"exp",         builtinExpF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"floor",       builtinFloorF32,            1,  {TOKEN_F32},                            TOKEN_F32},
    {"floor",       builtinFloorF64,            1,  {TOKEN_F64},                            TOKEN_F64},
    {"hypot",       builtinHypotF32,            2,  {TOKEN_F32, TOKEN_F32},                 TOKEN_F32},
    {"hypot",       builtinHypotF64,            2,  {TOKEN_F64, TOKEN_F64},                 TOKEN_F64},
    {"ldexp",       builtinLdexpF32,            2,  {TOKEN_F32, TOKEN_I32},                 TOKEN_F32},
    {"ldexp",       builtinLdexpF64,            2,  {TOKEN_F64, TOKEN_I32},                 TOKEN_F64},
    {"log",         builtinLogF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"log",         builtinLogF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"log10",       builtinLog10F32,            1,  {TOKEN_F32},                            TOKEN_F32},
    {"log10",       builtinLog10F64,            1,  {TOKEN_F64},                            TOKEN_F64},
    {"log2",        builtinLog2F32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"log2",        builtinLog2F64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"rad",         builtinRadF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"rad",         builtinRadF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"sin",         builtinSinF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"sin",         builtinSinF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"sinh",        builtinSinhF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"sinh",        builtinSinhF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"sqrt",        builtinSqrtF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"sqrt",        builtinSqrtF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"tan",         builtinTanF32,              1,  {TOKEN_F32},                            TOKEN_F32},
    {"tan",         builtinTanF64,              1,  {TOKEN_F64},                            TOKEN_F64},
    {"tanh",        builtinTanhF32,             1,  {TOKEN_F32},                            TOKEN_F32},
    {"tanh",        builtinTanhF64,             1,  {TOKEN_F64},                            TOKEN_F64},
    {"min",         builtinMinI32,             -2,  {TOKEN_I32},                            TOKEN_I32},
    {"min",         builtinMinI64,             -2,  {TOKEN_I64},                            TOKEN_I64},
    {"min",         builtinMinU32,             -2,  {TOKEN_U32},                            TOKEN_U32},
    {"min",         builtinMinU64,             -2,  {TOKEN_U64},                            TOKEN_U64},
    {"min",         builtinMinF32,             -2,  {TOKEN_F32},                            TOKEN_F32},
    {"min",         builtinMinF64,             -2,  {TOKEN_F64},                            TOKEN_F64},
    {"max",         builtinMaxI32,             -2,  {TOKEN_I32},                            TOKEN_I32},
    {"max",         builtinMaxI64,             -2,  {TOKEN_I64},                            TOKEN_I64},
    {"max",         builtinMaxU32,             -2,  {TOKEN_U32},                            TOKEN_U32},
    {"max",         builtinMaxU64,             -2,  {TOKEN_U64},                            TOKEN_U64},
    {"max",         builtinMaxF32,             -2,  {TOKEN_F32},                            TOKEN_F32},
    {"max",         builtinMaxF64,             -2,  {TOKEN_F64},                            TOKEN_F64},
    {"sum",         builtinSumI32,             -2,  {TOKEN_I32},                            TOKEN_I32},
    {"sum",         builtinSumI64,             -2,  {TOKEN_I64},                            TOKEN_I64},
    {"sum",         builtinSumI32,             -2,  {TOKEN_U32},                            TOKEN_U32},
    {"sum",         builtinSumI64,             -2,  {TOKEN_U64},                            TOKEN_U64},
    {"sum",         builtinSumF32,             -2,  {TOKEN_F32},                            TOKEN_F32},
    {"sum",         builtinSumF64,             -2,  {TOKEN_F64},                            TOKEN_F64},
    {"avg",         builtinAvgF32,             -2,  {TOKEN_F32},                            TOKEN_F32},
    {"avg",         builtinAvgF64,             -2,  {TOKEN_F64},                            TOKEN_F64},
    {"string",      builtinString,             -1,  {TOKEN_UNKNOWN},                        TOKEN_STRING}
};

static bool builtinArgumentMatches(Builtin* builtin, TokenType* argumentTypes, size_t position)
{
    TokenType argumentType = argumentTypes[position];
    TokenType parameterType = builtin->params[position];

    return argumentType == TOKEN_UNKNOWN
        || argumentType == parameterType
        || canImplicitlyWidenType(argumentType, parameterType);
}

static bool typedVariadicMatches(Builtin* builtin, TokenType* argumentTypes, size_t argumentCount, bool exact)
{
    if (!argumentCount || !argumentTypes) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < argumentCount && matches; i++) {
        TokenType argumentType = argumentTypes[i];
        TokenType parameterType = builtin->params[0];
        matches = argumentType == parameterType || (!exact && canImplicitlyWidenType(argumentType, parameterType));
    }

    return matches;
}

static bool builtinMatches(Builtin* builtin, TokenType* argumentTypes, size_t argumentCount)
{
    if (builtin->paramCount == BUILTIN_VARIADIC) {
        return true;
    }

    if (builtin->paramCount == BUILTIN_TYPED_VARIADIC) {
        return typedVariadicMatches(builtin, argumentTypes, argumentCount, false);
    }

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

static Builtin* matchExactTypedVariadicBuiltin(Builtin* builtin, TokenType* argumentTypes, size_t argumentCount)
{
    bool matches = typedVariadicMatches(builtin, argumentTypes, argumentCount, true);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static Builtin* matchExactBuiltin(Builtin* builtin, const char* name, TokenType* argumentTypes, size_t argumentCount)
{
    if (strcmp(name, builtin->name) != 0) {
        return NULL;
    }

    if (builtin->paramCount == BUILTIN_TYPED_VARIADIC) {
        return matchExactTypedVariadicBuiltin(builtin, argumentTypes, argumentCount);
    }

    if ((size_t)builtin->paramCount != argumentCount) {
        return NULL;
    }

    bool matches = true;

    for (size_t i = 0; i < argumentCount && matches; i++) {
        matches = argumentTypes[i] == builtin->params[i];
    }

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static Builtin* resolveExactBuiltin(const char* name, TokenType* argumentTypes, size_t argumentCount)
{
    Builtin* exact = NULL;

    for (int i = 0; i < BUILTINS_MAX && !exact; i++) {
        exact = matchExactBuiltin(&builtins[i], name, argumentTypes, argumentCount);
    }

    return exact;
}

Builtin* resolveBuiltin(const char* name, TokenType* argumentTypes, size_t argumentCount)
{
    Builtin* exact = resolveExactBuiltin(name, argumentTypes, argumentCount);

    if (exact) {
        return exact;
    }

    Builtin* variadic = NULL;

    for (int i = 0; i < BUILTINS_MAX; i++) {
        Builtin* builtin = &builtins[i];

        if (!builtinCallMatches(builtin, name, argumentTypes, argumentCount)) {
            continue;
        }

        if (builtin->paramCount == BUILTIN_VARIADIC) {
            variadic = builtin;

            continue;
        }

        return builtin;
    }

    return variadic;
}

bool isBuiltinName(const char* name)
{
    for (int i = 0; i < BUILTINS_MAX; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return true;
        }
    }

    return false;
}

void builtinByteorder(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t i = 1;
    char* c = (char*)&i;
    
    frame[-2] = SIGNED_VALUE(*c == 0);
}

void builtinExit(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;
    (void)frame;

    exit(0);
}

void builtinPrint(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    printValues(frame, function->argumentTypes, function->argumentCount);
    putchar('\n');
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

void builtinPrintU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    uint32_t n = AS_U32(frame[0]);

    printf("%" PRIu32 "\n", n);
}

void builtinPrintU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    uint64_t n = readU64(frame, 0);

    printf("%" PRIu64 "\n", n);
}

void builtinPrintF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float value = AS_F32(frame[0]);
    char buffer[32];

    formatShortestF32(buffer, sizeof(buffer), value);
    printf("%s\n", buffer);
}

void builtinPrintF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double value = readF64(frame, 0);
    char buffer[32];

    formatShortestF64(buffer, sizeof(buffer), value);
    printf("%s\n", buffer);
}

void builtinPrintString(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    StringObject* string = AS_POINTER(frame[0]);

    printStringObject(string);
    putchar('\n');
}

void builtinPrintChar(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    uint32_t value = AS_U32(frame[0]);
    char bytes[4];
    size_t length = encodeUtf8(value, bytes);

    fwrite(bytes, 1, length, stdout);
    putchar('\n');
}

void builtinPrintBool(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    printf("%s\n", AS_BOOL(frame[0]) ? "true" : "false");
}

void builtinClampI32(VM* vm, FunctionObject* function, Value* frame)
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

void builtinAbsI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_SIGNED(frame[0]);
    int32_t x = ABS(n);
    
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
    int64_t x = ABS(n);

    writeI64(frame, x);
}

void builtinAbsF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float n = AS_F32(frame[0]);
    float x = fabsf(n);

    frame[-2] = F32_VALUE(x);
}

void builtinAbsF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double n = readF64(frame, 0);
    double x = fabs(n);

    writeF64(frame, x);
}

void builtinMinI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int32_t result = AS_I32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int32_t value = AS_I32(frame[i]);
        result = MIN(result, value);
    }

    frame[-2] = I32_VALUE(result);
}

void builtinMinI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int64_t result = readI64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int64_t value = readI64(frame, i * 2);
        result = MIN(result, value);
    }

    writeI64(frame, result);
}

void builtinMaxI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int32_t result = AS_I32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int32_t value = AS_I32(frame[i]);
        result = MAX(result, value);
    }

    frame[-2] = I32_VALUE(result);
}

void builtinMaxI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int64_t result = readI64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int64_t value = readI64(frame, i * 2);
        result = MAX(result, value);
    }

    writeI64(frame, result);
}

void builtinPowI32(VM* vm, FunctionObject* function, Value* frame)
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

void builtinClampF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float c = AS_F32(frame[2]);
    float x = a;

    if (a < b) {
        x = b;
    } else if (a > c) {
        x = c;
    }

    frame[-2] = F32_VALUE(x);
}

void builtinClampF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double c = readF64(frame, 4);
    double x = a;

    if (a < b) {
        x = b;
    } else if (a > c) {
        x = c;
    }

    writeF64(frame, x);
}

void builtinMinF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = AS_F32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        float value = AS_F32(frame[i]);
        result = fminf(result, value);
    }

    frame[-2] = F32_VALUE(result);
}

void builtinMinF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = readF64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        double value = readF64(frame, i * 2);
        result = fmin(result, value);
    }

    writeF64(frame, result);
}

void builtinMaxF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = AS_F32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        float value = AS_F32(frame[i]);
        result = fmaxf(result, value);
    }

    frame[-2] = F32_VALUE(result);
}

void builtinMaxF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = readF64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        double value = readF64(frame, i * 2);
        result = fmax(result, value);
    }

    writeF64(frame, result);
}

void builtinPowF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = powf(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtinPowF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double x = pow(a, b);

    writeF64(frame, x);
}

void builtinAcosF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = acosf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAcosF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = acos(a);

    writeF64(frame, x);
}

void builtinAcoshF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = acoshf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAcoshF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = acosh(a);

    writeF64(frame, x);
}

void builtinAsinF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = asinf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAsinF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = asin(a);

    writeF64(frame, x);
}

void builtinAsinhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = asinhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAsinhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = asinh(a);

    writeF64(frame, x);
}

void builtinAtanF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = atanf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAtanF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = atan(a);

    writeF64(frame, x);
}

void builtinAtan2F32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = atan2f(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtinAtan2F64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double x = atan2(a, b);

    writeF64(frame, x);
}

void builtinAtanhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = atanhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAtanhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = atanh(a);

    writeF64(frame, x);
}

void builtinCbrtF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = cbrtf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCbrtF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = cbrt(a);

    writeF64(frame, x);
}

void builtinCeilF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = ceilf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCeilF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = ceil(a);

    writeF64(frame, x);
}

void builtinCosF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = cosf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCosF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = cos(a);

    writeF64(frame, x);
}

void builtinCoshF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = coshf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCoshF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = cosh(a);

    writeF64(frame, x);
}

void builtinDegF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = a * 57.2957795130823208768f;

    frame[-2] = F32_VALUE(x);
}

void builtinDegF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = a * 57.2957795130823208768;

    writeF64(frame, x);
}

void builtinExpF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = expf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinExpF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = exp(a);

    writeF64(frame, x);
}

void builtinFloorF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = floorf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinFloorF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = floor(a);

    writeF64(frame, x);
}

void builtinHypotF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = hypotf(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtinHypotF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double x = hypot(a, b);

    writeF64(frame, x);
}

void builtinLdexpF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    int32_t exponent = AS_I32(frame[1]);
    float x = ldexpf(a, exponent);

    frame[-2] = F32_VALUE(x);
}

void builtinLdexpF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    int32_t exponent = AS_I32(frame[2]);
    double x = ldexp(a, exponent);

    writeF64(frame, x);
}

void builtinLogF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = logf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinLogF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = log(a);

    writeF64(frame, x);
}

void builtinLog10F32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = log10f(a);

    frame[-2] = F32_VALUE(x);
}

void builtinLog10F64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = log10(a);

    writeF64(frame, x);
}

void builtinLog2F32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = log2f(a);

    frame[-2] = F32_VALUE(x);
}

void builtinLog2F64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = log2(a);

    writeF64(frame, x);
}

void builtinRadF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = a * 0.01745329251994329577f;

    frame[-2] = F32_VALUE(x);
}

void builtinRadF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = a * 0.01745329251994329577;

    writeF64(frame, x);
}

void builtinSinF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sinf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinSinF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = sin(a);

    writeF64(frame, x);
}

void builtinSinhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sinhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinSinhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = sinh(a);

    writeF64(frame, x);
}

void builtinSqrtF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sqrtf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinSqrtF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = sqrt(a);

    writeF64(frame, x);
}

void builtinTanF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = tanf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinTanF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = tan(a);

    writeF64(frame, x);
}

void builtinTanhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = tanhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinTanhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = tanh(a);

    writeF64(frame, x);
}

void builtinMinU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = AS_U32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint32_t value = AS_U32(frame[i]);
        result = MIN(result, value);
    }

    frame[-2] = U32_VALUE(result);
}

void builtinMinU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = readU64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint64_t value = readU64(frame, i * 2);
        result = MIN(result, value);
    }

    writeI64(frame, (int64_t)result);
}

void builtinMaxU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = AS_U32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint32_t value = AS_U32(frame[i]);
        result = MAX(result, value);
    }

    frame[-2] = U32_VALUE(result);
}

void builtinMaxU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = readU64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint64_t value = readU64(frame, i * 2);
        result = MAX(result, value);
    }

    writeI64(frame, (int64_t)result);
}

void builtinSumI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = 0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += AS_U32(frame[i]);
    }

    if (function->argumentTypes[0] == TOKEN_U32) {
        frame[-2] = U32_VALUE(result);

        return;
    }

    frame[-2] = I32_VALUE(result);
}

void builtinSumI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = 0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += readU64(frame, i * 2);
    }

    writeI64(frame, (int64_t)result);
}

void builtinSumF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = 0.0f;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += AS_F32(frame[i]);
    }

    frame[-2] = F32_VALUE(result);
}

void builtinSumF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = 0.0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += readF64(frame, i * 2);
    }

    writeF64(frame, result);
}

void builtinAvgF32(VM* vm, FunctionObject* function, Value* frame)
{
    builtinSumF32(vm, function, frame);

    float result = AS_F32(frame[-2]) / (float)function->argumentCount;

    frame[-2] = F32_VALUE(result);
}

void builtinAvgF64(VM* vm, FunctionObject* function, Value* frame)
{
    builtinSumF64(vm, function, frame);

    double result = readF64(frame - 2, 0);
    result /= (double)function->argumentCount;

    writeF64(frame, result);
}

void builtinString(VM* vm, FunctionObject* function, Value* frame)
{
    if (function->argumentCount == 1 && function->argumentTypes[0] == TOKEN_STRING) {
        frame[-2] = frame[0];

        return;
    }

    StringObject* string = concatenateStringParts(frame, function->argumentTypes, function->argumentCount);

    pushVectorItem(&vm->module->objects, string);

    frame[-2] = POINTER_VALUE(string);
}
