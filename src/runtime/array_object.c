#include "runtime/array_object.h"
#include "common/errors.h"

ArrayObject* create_array_object(size_t slots)
{
    if (slots > (SIZE_MAX - sizeof(ArrayObject)) / sizeof(Value)) {
        out_of_memory_error();
    }

    size_t bytes = sizeof(ArrayObject) + slots * sizeof(Value);
    ArrayObject* array = (ArrayObject*)allocate_object(bytes, ObjArray);
    array->count = slots;

    return array;
}
