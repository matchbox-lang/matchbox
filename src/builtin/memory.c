#include "builtin/memory.h"
#include "runtime/array_object.h"
#include "vm/vm.h"
#include <string.h>

void builtin_create_array(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;

    size_t size = AS_UNSIGNED(frame[0]);
    ArrayObject* array = create_array_object(size);

    push_vector_item(&vm->module->objects, array);

    frame[-2] = POINTER_VALUE(array->data);
}

void builtin_copy_array(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;

    const void* source = AS_POINTER(frame[0]);
    size_t size = AS_UNSIGNED(frame[1]);
    ArrayObject* array = create_array_object(size);

    memcpy(array->data, source, size);
    push_vector_item(&vm->module->objects, array);

    frame[-2] = POINTER_VALUE(array->data);
}
