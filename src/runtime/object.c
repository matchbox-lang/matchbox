#include "runtime/object.h"
#include "common/memory.h"
#include "common/util.h"
#include <stdlib.h>

Object* allocate_object(size_t size, ObjectType type)
{
    Object* object = (Object*)allocate_memory(size);
    object->type = type;
    
    return object;
}
