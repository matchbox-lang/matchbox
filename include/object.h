#ifndef OBJECT_H
#define OBJECT_H

#include "value.h"

#define AS_OBJECT(value) ((Object*)AS_POINTER(value))

typedef enum ObjectType
{
    OBJ_BOOL,
    OBJ_FLOAT,
    OBJ_INT,
    OBJ_STRING
} ObjectType;

typedef struct Object
{
    ObjectType type;
} Object;

#endif
