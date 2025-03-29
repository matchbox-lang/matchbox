#ifndef BOOL_OBJECT_H
#define BOOL_OBJECT_H

#include "object.h"

#define AS_BOOL_OBJECT(value) ((BoolObject*)AS_OBJECT(value))

typedef struct BoolObject
{
    Object obj;
    bool value;
} BoolObject;

BoolObject* createBoolObject(bool value);

#endif
