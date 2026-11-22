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

Type get_slice_type(Vector* array_types, Type element_type);

#endif
