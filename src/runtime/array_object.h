#ifndef ARRAY_OBJECT_H
#define ARRAY_OBJECT_H

#include "runtime/object.h"

typedef struct ArrayObject
{
    Object object;
    size_t count;
    uint8_t data[];
} ArrayObject;

ArrayObject* create_array_object(size_t size);

#endif
