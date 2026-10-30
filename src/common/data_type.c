#include "common/data_type.h"
#include <float.h>
#include <limits.h>

static bool acceptsNumericType(DataType source, DataType destination);
static bool acceptsBothNumericTypes(DataType left, DataType right, DataType destination);

const char* getDataTypeName(DataType type)
{
    switch (type) {
        case TYPE_I8:       return "i8";
        case TYPE_I16:      return "i16";
        case TYPE_I32:      return "i32";
        case TYPE_I64:      return "i64";
        case TYPE_U8:       return "u8";
        case TYPE_U16:      return "u16";
        case TYPE_U32:      return "u32";
        case TYPE_U64:      return "u64";
        case TYPE_F32:      return "f32";
        case TYPE_F64:      return "f64";
        case TYPE_CHAR:     return "char";
        case TYPE_STRING:   return "string";
        case TYPE_BOOL:     return "bool";
        case TYPE_VOID:     return "void";
        default:
            return "unknown token";
    }
}

bool isFloatType(DataType type)
{
    return type == TYPE_F32 || type == TYPE_F64;
}

bool isIntegerType(DataType type)
{
    return getIntegerTypeSize(type) != 0;
}

bool isSignedIntegerType(DataType type)
{
    switch (type) {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
            return true;
        default:
            return false;
    }
}

size_t getIntegerTypeSize(DataType type)
{
    switch (type) {
        case TYPE_I8:
        case TYPE_U8:
            return 1;
        case TYPE_I16:
        case TYPE_U16:
            return 2;
        case TYPE_I32:
        case TYPE_U32:
            return 4;
        case TYPE_I64:
        case TYPE_U64:
            return 8;
        default:
            return 0;
    }
}

size_t getTypeSlotCount(DataType type)
{
    return type == TYPE_I64 || type == TYPE_U64 || type == TYPE_F64 ? 2 : 1;
}

bool isPrintableType(DataType type)
{
    return isIntegerType(type) || isFloatType(type) || type == TYPE_BOOL || type == TYPE_CHAR || type == TYPE_STRING;
}

bool canImplicitlyWidenInteger(DataType source, DataType destination)
{
    if (!isIntegerType(source) || !isIntegerType(destination)) {
        return false;
    }

    if (isSignedIntegerType(source) && !isSignedIntegerType(destination)) {
        return false;
    }

    return getIntegerTypeSize(source) < getIntegerTypeSize(destination);
}

bool canImplicitlyWidenFloat(DataType source, DataType destination)
{
    return source == TYPE_F32 && destination == TYPE_F64;
}

bool canImplicitlyConvertIntegerToFloat(DataType source, DataType destination)
{
    if (!isIntegerType(source)) {
        return false;
    }

    size_t bits = getIntegerTypeSize(source) * CHAR_BIT;
    bool signedType = isSignedIntegerType(source);
    size_t precision = 0;

    if (destination == TYPE_F32) {
        precision = FLT_MANT_DIG;
    }

    if (destination == TYPE_F64) {
        precision = DBL_MANT_DIG;
    }

    return bits - signedType <= precision;
}

bool canImplicitlyWidenType(DataType source, DataType destination)
{
    if (canImplicitlyWidenInteger(source, destination)) {
        return true;
    }

    if (canImplicitlyConvertIntegerToFloat(source, destination)) {
        return true;
    }

    return canImplicitlyWidenFloat(source, destination);
}

bool canExplicitlyConvertType(DataType source, DataType destination)
{
    bool sourceNumeric = isIntegerType(source) || isFloatType(source);
    bool destinationNumeric = isIntegerType(destination) || isFloatType(destination);

    if (source == TYPE_BOOL && isIntegerType(destination)) {
        return true;
    }

    if (sourceNumeric && destinationNumeric) {
        return true;
    }

    if (source == TYPE_CHAR) {
        return destination == TYPE_CHAR || isIntegerType(destination);
    }

    return isIntegerType(source) && destination == TYPE_CHAR;
}

static bool acceptsNumericType(DataType source, DataType destination)
{
    return source == destination || canImplicitlyWidenType(source, destination);
}

static bool acceptsBothNumericTypes(DataType left, DataType right, DataType destination)
{
    bool acceptsLeft = acceptsNumericType(left, destination);
    bool acceptsRight = acceptsNumericType(right, destination);

    return acceptsLeft && acceptsRight;
}

DataType getCommonNumericType(DataType left, DataType right)
{
    bool leftNumeric = isIntegerType(left) || isFloatType(left);
    bool rightNumeric = isIntegerType(right) || isFloatType(right);

    if (!leftNumeric || !rightNumeric) {
        return TYPE_UNKNOWN;
    }

    if (acceptsNumericType(left, right)) {
        return right;
    }

    if (acceptsNumericType(right, left)) {
        return left;
    }

    static const DataType candidates[] = {
        TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64, TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64, TYPE_F32, TYPE_F64
    };
    
    size_t count = sizeof(candidates) / sizeof(*candidates);
    size_t index = 0;

    while (index < count && !acceptsBothNumericTypes(left, right, candidates[index])) {
        index++;
    }

    if (index == count) {
        return TYPE_UNKNOWN;
    }

    return candidates[index];
}
