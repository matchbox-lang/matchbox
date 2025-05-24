#ifndef OBJECT_H
#define OBJECT_H

#include "value.h"
#include <stdlib.h>

#define ALLOCATE_OBJECT(type, objectType) (type*)allocateObject(sizeof(type), objectType)
#define AS_OBJECT(value) ((Object*)AS_POINTER(value))

typedef enum ObjectType
{
    OBJ_BOOL,
    OBJ_CODE,
    OBJ_FLOAT,
    OBJ_INT,
    OBJ_FUNCTION,
    OBJ_MODULE,
    OBJ_STRING
} ObjectType;

typedef struct Object
{
    ObjectType type;
} Object;

Object* allocateObject(size_t size, ObjectType type);

#endif
