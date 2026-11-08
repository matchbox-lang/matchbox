#ifndef ARRAY_FORMAT_H
#define ARRAY_FORMAT_H

#include "runtime/argument_format.h"
#include "runtime/object.h"

typedef struct ArrayFormat
{
    Object object;
    size_t count;
    size_t stride;
    ArgumentFormat element;
} ArrayFormat;

#endif
