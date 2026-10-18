#include "builtin.h"
#include "float_format.h"
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

static void interpolateI8(VM* vm, FunctionObject* function, Value* frame);
static void interpolateI16(VM* vm, FunctionObject* function, Value* frame);
static void interpolateI32(VM* vm, FunctionObject* function, Value* frame);
static void interpolateI64(VM* vm, FunctionObject* function, Value* frame);
static void interpolateU8(VM* vm, FunctionObject* function, Value* frame);
static void interpolateU16(VM* vm, FunctionObject* function, Value* frame);
static void interpolateU32(VM* vm, FunctionObject* function, Value* frame);
static void interpolateU64(VM* vm, FunctionObject* function, Value* frame);
static void interpolateF32(VM* vm, FunctionObject* function, Value* frame);
static void interpolateF64(VM* vm, FunctionObject* function, Value* frame);
static void interpolateBool(VM* vm, FunctionObject* function, Value* frame);
static void interpolateChar(VM* vm, FunctionObject* function, Value* frame);
static void interpolateString(VM* vm, FunctionObject* function, Value* frame);

Builtin builtins[BUILTINS_MAX] = {
    {"exit",        builtinExit,                0,  {},                                 TOKEN_VOID},
    {"print",       builtinPrintI32,            1,  {TOKEN_I32},                        TOKEN_VOID},
    {"print",       builtinPrintI64,            1,  {TOKEN_I64},                        TOKEN_VOID},
    {"print",       builtinPrintU32,            1,  {TOKEN_U32},                        TOKEN_VOID},
    {"print",       builtinPrintU64,            1,  {TOKEN_U64},                        TOKEN_VOID},
    {"print",       builtinPrintF32,            1,  {TOKEN_F32},                        TOKEN_VOID},
    {"print",       builtinPrintF64,            1,  {TOKEN_F64},                        TOKEN_VOID},
    {"print",       builtinPrintBool,           1,  {TOKEN_BOOL},                       TOKEN_VOID},
    {"print",       builtinPrintChar,           1,  {TOKEN_CHAR},                       TOKEN_VOID},
    {"print",       builtinPrintString,         1,  {TOKEN_STRING},                     TOKEN_VOID},
    {"clamp",       builtinClamp,               3,  {TOKEN_I32, TOKEN_I32, TOKEN_I32},  TOKEN_I32},
    {"clamp",       builtinClampI64,            3,  {TOKEN_I64, TOKEN_I64, TOKEN_I64},  TOKEN_I64},
    {"abs",         builtinAbs,                 1,  {TOKEN_I32},                        TOKEN_I32},
    {"abs",         builtinAbsI64,              1,  {TOKEN_I64},                        TOKEN_I64},
    {"min",         builtinMin,                 2,  {TOKEN_I32, TOKEN_I32},             TOKEN_I32},
    {"min",         builtinMinI64,              2,  {TOKEN_I64, TOKEN_I64},             TOKEN_I64},
    {"max",         builtinMax,                 2,  {TOKEN_I32, TOKEN_I32},             TOKEN_I32},
    {"max",         builtinMaxI64,              2,  {TOKEN_I64, TOKEN_I64},             TOKEN_I64},
    {"pow",         builtinPow,                 2,  {TOKEN_I32, TOKEN_I32},             TOKEN_I32},
    {"pow",         builtinPowI64,              2,  {TOKEN_I64, TOKEN_I64},             TOKEN_I64},
    {"byteorder",   builtinByteorder,           0,  {},                                 TOKEN_I32},
    {"concat",      builtinConcatStrings,       2,  {TOKEN_STRING, TOKEN_STRING},       TOKEN_STRING},
    {"concat",      builtinConcatStringChar,    2,  {TOKEN_STRING, TOKEN_CHAR},         TOKEN_STRING},
    {"concat",      builtinConcatCharString,    2,  {TOKEN_CHAR, TOKEN_STRING},         TOKEN_STRING},
    {"concat",      builtinConcatChars,         2,  {TOKEN_CHAR, TOKEN_CHAR},           TOKEN_STRING}
};

static bool builtinArgumentMatches(Builtin* builtin, TokenType* argumentTypes, size_t position)
{
    TokenType argumentType = argumentTypes[position];
    TokenType parameterType = builtin->params[position];

    return argumentType == TOKEN_UNKNOWN
        || argumentType == parameterType
        || canImplicitlyWidenType(argumentType, parameterType);
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

bool isBuiltinName(const char* name)
{
    for (int i = 0; i < BUILTINS_MAX; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return true;
        }
    }

    return false;
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

static void builtinConcat(VM* vm, Value* frame, TokenType leftType, TokenType rightType)
{
    StringObject* string = concatenateStringValues(frame[0], frame[1], leftType, rightType);

    pushVectorItem(&vm->module->objects, string);
    
    frame[-2] = POINTER_VALUE(string);
}

void builtinConcatStrings(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    builtinConcat(vm, frame, TOKEN_STRING, TOKEN_STRING);
}

void builtinConcatStringChar(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    builtinConcat(vm, frame, TOKEN_STRING, TOKEN_CHAR);
}

void builtinConcatCharString(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    builtinConcat(vm, frame, TOKEN_CHAR, TOKEN_STRING);
}

void builtinConcatChars(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    builtinConcat(vm, frame, TOKEN_CHAR, TOKEN_CHAR);
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

void builtinMin(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = MIN(a, b);
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtinMinI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = readI64(frame, 0);
    int64_t b = readI64(frame, 2);
    int64_t x = MIN(a, b);

    writeI64(frame, x);
}

void builtinMax(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = MAX(a, b);
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtinMaxI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = readI64(frame, 0);
    int64_t b = readI64(frame, 2);
    int64_t x = MAX(a, b);

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

static size_t formatInterpolatedValue(char* bytes, size_t capacity, Value* frame, TokenType type)
{
    switch (type) {
        case TOKEN_I8:
        case TOKEN_I16:
        case TOKEN_I32: {
            int32_t value = AS_I32(frame[0]);

            return snprintf(bytes, capacity, "%" PRId32, value);
        }
        case TOKEN_I64: {
            int64_t value = readI64(frame, 0);

            return snprintf(bytes, capacity, "%" PRId64, value);
        }
        case TOKEN_U8:
        case TOKEN_U16:
        case TOKEN_U32: {
            uint32_t value = AS_U32(frame[0]);

            return snprintf(bytes, capacity, "%" PRIu32, value);
        }
        case TOKEN_U64: {
            uint64_t value = readU64(frame, 0);

            return snprintf(bytes, capacity, "%" PRIu64, value);
        }
        case TOKEN_F32: {
            float value = AS_F32(frame[0]);

            formatShortestF32(bytes, capacity, value);

            return strlen(bytes);
        }
        case TOKEN_F64: {
            double value = readF64(frame, 0);

            formatShortestF64(bytes, capacity, value);

            return strlen(bytes);
        }
        case TOKEN_BOOL: {
            const char* value = "false";

            if (AS_BOOL(frame[0])) {
                value = "true";
            }

            return snprintf(bytes, capacity, "%s", value);
        }
        case TOKEN_CHAR: {
            uint32_t value = AS_U32(frame[0]);

            return encodeUtf8(value, bytes);
        }
        default:
            return 0;
    }
}

static void interpolateValue(VM* vm, Value* frame, TokenType type)
{
    if (type == TOKEN_STRING) {
        frame[-2] = frame[0];

        return;
    }

    char bytes[64];
    size_t length = formatInterpolatedValue(bytes, sizeof(bytes), frame, type);
    StringObject* string = copyStringObject(bytes, length);

    pushVectorItem(&vm->module->objects, string);

    frame[-2] = POINTER_VALUE(string);
}

static void interpolateI8(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_I8);
}

static void interpolateI16(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_I16);
}

static void interpolateI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_I32);
}

static void interpolateI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_I64);
}

static void interpolateU8(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_U8);
}

static void interpolateU16(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_U16);
}

static void interpolateU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_U32);
}

static void interpolateU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_U64);
}

static void interpolateF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_F32);
}

static void interpolateF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_F64);
}

static void interpolateBool(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_BOOL);
}

static void interpolateChar(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_CHAR);
}

static void interpolateString(VM* vm, FunctionObject* function, Value* frame)
{
    (void)function;

    interpolateValue(vm, frame, TOKEN_STRING);
}

Builtin* getInterpolationBuiltin(TokenType type)
{
    static Builtin conversions[] = {
        {"interpolation", interpolateI8, 1, {TOKEN_I8}, TOKEN_STRING},
        {"interpolation", interpolateI16, 1, {TOKEN_I16}, TOKEN_STRING},
        {"interpolation", interpolateI32, 1, {TOKEN_I32}, TOKEN_STRING},
        {"interpolation", interpolateI64, 1, {TOKEN_I64}, TOKEN_STRING},
        {"interpolation", interpolateU8, 1, {TOKEN_U8}, TOKEN_STRING},
        {"interpolation", interpolateU16, 1, {TOKEN_U16}, TOKEN_STRING},
        {"interpolation", interpolateU32, 1, {TOKEN_U32}, TOKEN_STRING},
        {"interpolation", interpolateU64, 1, {TOKEN_U64}, TOKEN_STRING},
        {"interpolation", interpolateF32, 1, {TOKEN_F32}, TOKEN_STRING},
        {"interpolation", interpolateF64, 1, {TOKEN_F64}, TOKEN_STRING},
        {"interpolation", interpolateBool, 1, {TOKEN_BOOL}, TOKEN_STRING},
        {"interpolation", interpolateChar, 1, {TOKEN_CHAR}, TOKEN_STRING},
        {"interpolation", interpolateString, 1, {TOKEN_STRING}, TOKEN_STRING}
    };
    size_t count = sizeof(conversions) / sizeof(conversions[0]);

    for (size_t i = 0; i < count; i++) {
        if (conversions[i].params[0] == type) {
            return &conversions[i];
        }
    }

    return NULL;
}
