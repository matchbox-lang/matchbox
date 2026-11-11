#include "runtime/array_object.h"
#include "common/errors.h"

ArrayObject* create_array_object(size_t size)
{
    if (size > SIZE_MAX - sizeof(ArrayObject)) {
        out_of_memory_error();
    }

    size_t bytes = sizeof(ArrayObject) + size;
    ArrayObject* array = (ArrayObject*)allocate_object(bytes, ObjArray);

    array->count = size;

    return array;
}
