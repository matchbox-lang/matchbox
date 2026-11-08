#ifndef ARRAY_TYPE_H
#define ARRAY_TYPE_H

#include "common/data_type.h"

typedef struct ArrayType
{
    DataType element_type;
    size_t count;
    size_t slots;
    struct ArrayType* next;
    char name[];
} ArrayType;

bool is_array_type(DataType type);
DataType get_array_type(DataType element_type, size_t count);
const ArrayType* get_array_type_info(DataType type);
DataType get_array_leaf_type(DataType type);
size_t get_array_storage_slots(DataType type);

#endif
