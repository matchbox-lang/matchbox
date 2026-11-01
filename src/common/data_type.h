#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum DataType
{
    TypeUnknown,
    TypeI8,
    TypeI16,
    TypeI32,
    TypeI64,
    TypeU8,
    TypeU16,
    TypeU32,
    TypeU64,
    TypeIsize,
    TypeUsize,
    TypeF32,
    TypeF64,
    TypeChar,
    TypeString,
    TypeBool,
    TypeVoid
} DataType;

const char* get_data_type_name(DataType type);
bool is_integer_type(DataType type);
bool is_float_type(DataType type);
bool is_signed_integer_type(DataType type);
size_t get_integer_type_size(DataType type);
size_t get_type_slot_count(DataType type);
bool is_printable_type(DataType type);
bool can_implicitly_widen_integer(DataType source, DataType destination);
bool can_implicitly_convert_integer_to_float(DataType source, DataType destination);
bool can_implicitly_widen_float(DataType source, DataType destination);
bool can_implicitly_widen_type(DataType source, DataType destination);
bool can_explicitly_convert_type(DataType source, DataType destination);
DataType get_common_numeric_type(DataType left, DataType right);

#endif
