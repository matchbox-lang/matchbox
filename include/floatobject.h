#ifndef FLOAT_OBJECT_H
#define FLOAT_OBJECT_H

#include "object.h"

#define AS_FLOAT_OBJECT(value) ((FloatObject*)AS_OBJECT(value))

typedef struct FloatObject
{
    Object obj;
    float floatValue;
} FloatObject;

#endif
