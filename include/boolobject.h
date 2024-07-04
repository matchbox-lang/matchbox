#ifndef BOOLOBJECT_H
#define BOOLOBJECT_H

#include "object.h"
#include "value.h"

#define AS_BOOLOBJECT(value) ((BoolObject*)AS_POINTER(value))

typedef struct BoolObject
{
    Object obj;
    bool boolVal;
} BoolObject;

#endif
