#include "object.h"
#include <stdlib.h>

Object* allocateObject(size_t size, ObjectType type)
{
    Object* object = (Object*)malloc(size);
    object->type = type;
    
    return object;
}
