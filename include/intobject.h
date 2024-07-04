#ifndef INTOBJECT_H
#define INTOBJECT_H

#include "object.h"
#include "value.h"

#define AS_INTOBJECT(value) ((IntObject*)AS_POINTER(value))

typedef struct IntObject
{
    Object obj;
    int intVal;
} IntObject;

#endif
