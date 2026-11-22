#include "common/slice.h"
#include "common/array_type.h"
#include "common/memory.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

Type get_slice_type(Vector* array_types, Type element_type)
{
    Type type = find_array_type(array_types, element_type, SIZE_MAX);

    if (type.kind != TypeUnknown) {
        type.kind = TypeSlice;

        return type;
    }

    const char* name = get_data_type_name(element_type);
    size_t length = strlen(name) + 3;
    ArrayType* info = allocate_zeroed_memory(1, sizeof(ArrayType) + length);
    info->element_type = element_type;
    info->count = SIZE_MAX;
    info->size = sizeof(Slice);

    snprintf(info->name, length, "%s[]", name);
    push_vector_item(array_types, info);

    return (Type){.kind = TypeSlice, .array = info};
}
