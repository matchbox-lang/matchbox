#ifndef SLICE_H
#define SLICE_H

#include "common/type.h"
#include "common/vector.h"
#include <stddef.h>

typedef struct Slice
{
    void* data;
    size_t length;
} Slice;

bool can_slice_type(Type source, Type destination);
Type get_slice_type(Vector* array_types, Type element_type);
Type get_string_slice_type(Vector* array_types);

#endif
