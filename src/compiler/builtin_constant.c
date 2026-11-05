#include "compiler/builtin_constant.h"
#include "compiler/constant.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct BuiltinConstant
{
    DataType type;
    const char* name;
    ConstantValue value;
} BuiltinConstant;

static const BuiltinConstant builtin_constants[] = {
    {TypeI8,        "MIN",              {.integer = (uint64_t)INT8_MIN}},
    {TypeI8,        "MAX",              {.integer = (uint64_t)INT8_MAX}},
    {TypeI16,       "MIN",              {.integer = (uint64_t)INT16_MIN}},
    {TypeI16,       "MAX",              {.integer = (uint64_t)INT16_MAX}},
    {TypeI32,       "MIN",              {.integer = (uint64_t)INT32_MIN}},
    {TypeI32,       "MAX",              {.integer = (uint64_t)INT32_MAX}},
    {TypeI64,       "MIN",              {.integer = (uint64_t)INT64_MIN}},
    {TypeI64,       "MAX",              {.integer = (uint64_t)INT64_MAX}},
    {TypeIsize,     "MIN",              {.integer = (uint64_t)INTPTR_MIN}},
    {TypeIsize,     "MAX",              {.integer = (uint64_t)INTPTR_MAX}},
    {TypeU8,        "MIN",              {.integer = 0}},
    {TypeU8,        "MAX",              {.integer = UINT8_MAX}},
    {TypeU16,       "MIN",              {.integer = 0}},
    {TypeU16,       "MAX",              {.integer = UINT16_MAX}},
    {TypeU32,       "MIN",              {.integer = 0}},
    {TypeU32,       "MAX",              {.integer = UINT32_MAX}},
    {TypeU64,       "MIN",              {.integer = 0}},
    {TypeU64,       "MAX",              {.integer = UINT64_MAX}},
    {TypeUsize,     "MIN",              {.integer = 0}},
    {TypeUsize,     "MAX",              {.integer = UINTPTR_MAX}},
    {TypeF32,       "MIN",              {.floating = -FLT_MAX}},
    {TypeF32,       "MAX",              {.floating = FLT_MAX}},
    {TypeF32,       "EPSILON",          {.floating = FLT_EPSILON}},
    {TypeF32,       "MIN_POSITIVE",     {.floating = FLT_MIN}},
    {TypeF32,       "INFINITY",         {.floating = INFINITY}},
    {TypeF32,       "NEG_INFINITY",     {.floating = -INFINITY}},
    {TypeF32,       "NAN",              {.floating = NAN}},
    {TypeF64,       "MIN",              {.floating = -DBL_MAX}},
    {TypeF64,       "MAX",              {.floating = DBL_MAX}},
    {TypeF64,       "EPSILON",          {.floating = DBL_EPSILON}},
    {TypeF64,       "MIN_POSITIVE",     {.floating = DBL_MIN}},
    {TypeF64,       "INFINITY",         {.floating = INFINITY}},
    {TypeF64,       "NEG_INFINITY",     {.floating = -INFINITY}},
    {TypeF64,       "NAN",              {.floating = NAN}},
    {TypeChar,      "MIN",              {.integer = 0}},
    {TypeChar,      "MAX",              {.integer = 0x10FFFF}}
};

static bool matches_builtin_constant(const BuiltinConstant* constant, DataType type, Token name)
{
    size_t length = strlen(constant->name);

    return constant->type == type && length == name.length && memcmp(constant->name, name.chars, length) == 0;
}

ASTNode* resolve_builtin_constant(DataType type, Token name)
{
    size_t count = sizeof(builtin_constants) / sizeof(*builtin_constants);
    size_t index = 0;

    while (index < count && !matches_builtin_constant(&builtin_constants[index], type, name)) {
        index++;
    }

    if (index == count) {
        return NULL;
    }

    ASTNode* literal = create_constant_node(builtin_constants[index].value, type, name);

    return create_conversion_node(name, literal, type);
}
