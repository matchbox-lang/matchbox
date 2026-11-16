#include "runtime/object.h"
#include "common/memory.h"

Object* allocate_object(size_t size, ObjectType type)
{
    Object* object = (Object*)allocate_memory(size);
    object->type = type;
    
    return object;
}
