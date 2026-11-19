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
bool array_shapes_equal(Type left, Type right);
// Returns TypeUnknown when the array type is not registered
Type find_array_type(const Vector* array_types, Type element_type, size_t count);
Type add_array_type(Vector* array_types, Type element_type, size_t count);
Type get_array_leaf_type(Type type);
size_t get_array_storage_size(Type type);

#endif
