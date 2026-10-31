#include "runtime/object.h"
#include "common/memory.h"
#include "common/util.h"
#include <stdlib.h>

Object* allocateObject(size_t size, ObjectType type)
{
    Object* object = (Object*)allocateMemory(size);
    object->type = type;
    
    return object;
}
