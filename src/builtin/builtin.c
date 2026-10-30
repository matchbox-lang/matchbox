#include "builtin/builtin.h"
#include "builtin/io.h"
#include "builtin/math.h"
#include "builtin/string.h"
#include "builtin/system.h"
#include "common/data_type.h"
#include <string.h>

Builtin builtins[BUILTINS_MAX] = {
    // I/O
    {"print",       builtinPrint,              -1,  {TYPE_UNKNOWN},                         TYPE_VOID},

    // Math
    {"abs",         builtinAbsI32,              1,  {TYPE_I32},                             TYPE_I32},
    {"abs",         builtinAbsI64,              1,  {TYPE_I64},                             TYPE_I64},
    {"abs",         builtinAbsF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"abs",         builtinAbsF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"acos",        builtinAcosF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"acos",        builtinAcosF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"acosh",       builtinAcoshF32,            1,  {TYPE_F32},                             TYPE_F32},
    {"acosh",       builtinAcoshF64,            1,  {TYPE_F64},                             TYPE_F64},
    {"asin",        builtinAsinF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"asin",        builtinAsinF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"asinh",       builtinAsinhF32,            1,  {TYPE_F32},                             TYPE_F32},
    {"asinh",       builtinAsinhF64,            1,  {TYPE_F64},                             TYPE_F64},
    {"atan",        builtinAtanF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"atan",        builtinAtanF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"atan2",       builtinAtan2F32,            2,  {TYPE_F32, TYPE_F32},                   TYPE_F32},
    {"atan2",       builtinAtan2F64,            2,  {TYPE_F64, TYPE_F64},                   TYPE_F64},
    {"atanh",       builtinAtanhF32,            1,  {TYPE_F32},                             TYPE_F32},
    {"atanh",       builtinAtanhF64,            1,  {TYPE_F64},                             TYPE_F64},
    {"avg",         builtinAvgF32,             -2,  {TYPE_F32},                             TYPE_F32},
    {"avg",         builtinAvgF64,             -2,  {TYPE_F64},                             TYPE_F64},
    {"cbrt",        builtinCbrtF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"cbrt",        builtinCbrtF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"ceil",        builtinCeilF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"ceil",        builtinCeilF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"clamp",       builtinClampI32,            3,  {TYPE_I32, TYPE_I32, TYPE_I32},         TYPE_I32},
    {"clamp",       builtinClampI64,            3,  {TYPE_I64, TYPE_I64, TYPE_I64},         TYPE_I64},
    {"clamp",       builtinClampF32,            3,  {TYPE_F32, TYPE_F32, TYPE_F32},         TYPE_F32},
    {"clamp",       builtinClampF64,            3,  {TYPE_F64, TYPE_F64, TYPE_F64},         TYPE_F64},
    {"cos",         builtinCosF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"cos",         builtinCosF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"cosh",        builtinCoshF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"cosh",        builtinCoshF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"deg",         builtinDegF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"deg",         builtinDegF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"exp",         builtinExpF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"exp",         builtinExpF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"floor",       builtinFloorF32,            1,  {TYPE_F32},                             TYPE_F32},
    {"floor",       builtinFloorF64,            1,  {TYPE_F64},                             TYPE_F64},
    {"hypot",       builtinHypotF32,            2,  {TYPE_F32, TYPE_F32},                   TYPE_F32},
    {"hypot",       builtinHypotF64,            2,  {TYPE_F64, TYPE_F64},                   TYPE_F64},
    {"ldexp",       builtinLdexpF32,            2,  {TYPE_F32, TYPE_I32},                   TYPE_F32},
    {"ldexp",       builtinLdexpF64,            2,  {TYPE_F64, TYPE_I32},                   TYPE_F64},
    {"log",         builtinLogF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"log",         builtinLogF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"log10",       builtinLog10F32,            1,  {TYPE_F32},                             TYPE_F32},
    {"log10",       builtinLog10F64,            1,  {TYPE_F64},                             TYPE_F64},
    {"log2",        builtinLog2F32,             1,  {TYPE_F32},                             TYPE_F32},
    {"log2",        builtinLog2F64,             1,  {TYPE_F64},                             TYPE_F64},
    {"max",         builtinMaxI32,             -2,  {TYPE_I32},                             TYPE_I32},
    {"max",         builtinMaxI64,             -2,  {TYPE_I64},                             TYPE_I64},
    {"max",         builtinMaxU32,             -2,  {TYPE_U32},                             TYPE_U32},
    {"max",         builtinMaxU64,             -2,  {TYPE_U64},                             TYPE_U64},
    {"max",         builtinMaxF32,             -2,  {TYPE_F32},                             TYPE_F32},
    {"max",         builtinMaxF64,             -2,  {TYPE_F64},                             TYPE_F64},
    {"min",         builtinMinI32,             -2,  {TYPE_I32},                             TYPE_I32},
    {"min",         builtinMinI64,             -2,  {TYPE_I64},                             TYPE_I64},
    {"min",         builtinMinU32,             -2,  {TYPE_U32},                             TYPE_U32},
    {"min",         builtinMinU64,             -2,  {TYPE_U64},                             TYPE_U64},
    {"min",         builtinMinF32,             -2,  {TYPE_F32},                             TYPE_F32},
    {"min",         builtinMinF64,             -2,  {TYPE_F64},                             TYPE_F64},
    {"pow",         builtinPowI32,              2,  {TYPE_I32, TYPE_I32},                   TYPE_I32},
    {"pow",         builtinPowI64,              2,  {TYPE_I64, TYPE_I64},                   TYPE_I64},
    {"pow",         builtinPowF32,              2,  {TYPE_F32, TYPE_F32},                   TYPE_F32},
    {"pow",         builtinPowF64,              2,  {TYPE_F64, TYPE_F64},                   TYPE_F64},
    {"rad",         builtinRadF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"rad",         builtinRadF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"sin",         builtinSinF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"sin",         builtinSinF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"sinh",        builtinSinhF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"sinh",        builtinSinhF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"sqrt",        builtinSqrtF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"sqrt",        builtinSqrtF64,             1,  {TYPE_F64},                             TYPE_F64},
    {"sum",         builtinSumI32,             -2,  {TYPE_I32},                             TYPE_I32},
    {"sum",         builtinSumI64,             -2,  {TYPE_I64},                             TYPE_I64},
    {"sum",         builtinSumU32,             -2,  {TYPE_U32},                             TYPE_U32},
    {"sum",         builtinSumU64,             -2,  {TYPE_U64},                             TYPE_U64},
    {"sum",         builtinSumF32,             -2,  {TYPE_F32},                             TYPE_F32},
    {"sum",         builtinSumF64,             -2,  {TYPE_F64},                             TYPE_F64},
    {"tan",         builtinTanF32,              1,  {TYPE_F32},                             TYPE_F32},
    {"tan",         builtinTanF64,              1,  {TYPE_F64},                             TYPE_F64},
    {"tanh",        builtinTanhF32,             1,  {TYPE_F32},                             TYPE_F32},
    {"tanh",        builtinTanhF64,             1,  {TYPE_F64},                             TYPE_F64},

    // String
    {"string",      builtinString,             -1,  {TYPE_UNKNOWN},                         TYPE_STRING},

    // System
    {"byteorder",   builtinByteorder,           0,  {},                                     TYPE_I32},
    {"exit",        builtinExit,                0,  {},                                     TYPE_VOID},
    {"exit",        builtinExitStatus,          1,  {TYPE_I32},                             TYPE_VOID}
};

static bool builtinArgumentMatches(Builtin* builtin, DataType* argumentTypes, size_t position)
{
    DataType argumentType = argumentTypes[position];
    DataType parameterType = builtin->params[position];

    return argumentType == TYPE_UNKNOWN
        || argumentType == parameterType
        || canImplicitlyWidenType(argumentType, parameterType);
}

static bool typedVariadicMatches(Builtin* builtin, DataType* argumentTypes, size_t argumentCount, bool exact)
{
    if (!argumentCount || !argumentTypes) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < argumentCount && matches; i++) {
        DataType argumentType = argumentTypes[i];
        DataType parameterType = builtin->params[0];
        matches = argumentType == parameterType || (!exact && canImplicitlyWidenType(argumentType, parameterType));
    }

    return matches;
}

static bool builtinMatches(Builtin* builtin, DataType* argumentTypes, size_t argumentCount)
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
    Builtin* builtin, const char* name, DataType* argumentTypes, size_t argumentCount)
{
    return strcmp(name, builtin->name) == 0
        && builtinMatches(builtin, argumentTypes, argumentCount);
}

static Builtin* matchExactTypedVariadicBuiltin(Builtin* builtin, DataType* argumentTypes, size_t argumentCount)
{
    bool matches = typedVariadicMatches(builtin, argumentTypes, argumentCount, true);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static Builtin* matchExactBuiltin(Builtin* builtin, const char* name, DataType* argumentTypes, size_t argumentCount)
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

static Builtin* resolveExactBuiltin(const char* name, DataType* argumentTypes, size_t argumentCount)
{
    Builtin* exact = NULL;

    for (int i = 0; i < BUILTINS_MAX && !exact; i++) {
        exact = matchExactBuiltin(&builtins[i], name, argumentTypes, argumentCount);
    }

    return exact;
}

Builtin* resolveBuiltin(const char* name, DataType* argumentTypes, size_t argumentCount)
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
