#ifndef FLOATOBJECT_H
#define FLOATOBJECT_H

#include "object.h"
#include "value.h"

#define AS_FLOATOBJECT(value) ((FloatObject*)AS_POINTER(value))

typedef struct FloatObject
{
    Object obj;
    float floatVal;
} FloatObject;

#endif
