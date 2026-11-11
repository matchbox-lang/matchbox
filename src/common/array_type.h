#ifndef ARRAY_TYPE_H
#define ARRAY_TYPE_H

#include "common/type.h"
#include "common/vector.h"

typedef struct ArrayType
{
    Type element_type;
    Vector* dimensions;
    size_t count;
    size_t size;
    char name[];
} ArrayType;

bool is_array_type(TypeKind kind);
Type get_array_type(Vector* array_types, Type element_type, size_t count);
Type get_array_leaf_type(Type type);
size_t get_array_storage_size(Type type);

#endif
