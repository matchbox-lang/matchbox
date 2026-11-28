#ifndef TUPLE_FORMAT_H
#define TUPLE_FORMAT_H

#include "runtime/argument_format.h"
#include "runtime/object.h"

typedef struct TupleFieldFormat
{
    ArgumentFormat format;
    size_t size;
    bool reference;
} TupleFieldFormat;

typedef struct TupleFormat
{
    Object object;
    size_t count;
    TupleFieldFormat fields[];
} TupleFormat;

#endif
