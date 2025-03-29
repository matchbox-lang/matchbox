#ifndef INT_OBJECT_H
#define INT_OBJECT_H

#include "object.h"

#define AS_INT_OBJECT(value) ((IntObject*)AS_OBJECT(value))

typedef struct IntObject
{
    Object obj;
    int intValue;
} IntObject;

#endif
