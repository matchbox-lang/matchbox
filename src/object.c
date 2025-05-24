#include "object.h"
#include "functionobject.h"
#include "stringobject.h"

Object* allocateObject(size_t size, ObjectType type)
{
    Object* object = (Object*)malloc(size);
    object->type = type;
    
    return object;
}
