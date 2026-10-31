#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum DataType
{
    TYPE_UNKNOWN,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_ISIZE,
    TYPE_USIZE,
    TYPE_F32,
    TYPE_F64,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID
} DataType;

const char* getDataTypeName(DataType type);
bool isIntegerType(DataType type);
bool isFloatType(DataType type);
bool isSignedIntegerType(DataType type);
size_t getIntegerTypeSize(DataType type);
size_t getTypeSlotCount(DataType type);
bool isPrintableType(DataType type);
bool canImplicitlyWidenInteger(DataType source, DataType destination);
bool canImplicitlyConvertIntegerToFloat(DataType source, DataType destination);
bool canImplicitlyWidenFloat(DataType source, DataType destination);
bool canImplicitlyWidenType(DataType source, DataType destination);
bool canExplicitlyConvertType(DataType source, DataType destination);
DataType getCommonNumericType(DataType left, DataType right);

#endif
