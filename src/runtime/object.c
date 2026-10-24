#include "runtime/object.h"
#include "common/errors.h"
#include "common/util.h"
#include <stdlib.h>

Object* allocateObject(size_t size, ObjectType type)
{
    Object* object = (Object*)malloc(size);
    if (!object) {
        outOfMemoryError();
    }

    object->type = type;
    
    return object;
}
