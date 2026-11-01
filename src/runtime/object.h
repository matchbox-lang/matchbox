#ifndef OBJECT_H
#define OBJECT_H

#include "runtime/value.h"
#include <stdlib.h>

#define ALLOCATE_OBJECT(type, object_type) (type*)allocate_object(sizeof(type), object_type)
#define AS_OBJECT(value) ((Object*)AS_POINTER(value))

typedef enum ObjectType
{
    ObjBool,
    ObjCode,
    ObjFloat,
    ObjInt,
    ObjFunction,
    ObjModule,
    ObjString
} ObjectType;

typedef struct Object
{
    ObjectType type;
} Object;

Object* allocate_object(size_t size, ObjectType type);

#endif
