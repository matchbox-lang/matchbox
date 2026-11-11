#include "common/array_type.h"
#include "common/errors.h"
#include "common/memory.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static ArrayType* find_array_type(const Vector* array_types, Type element_type, size_t count);
static bool matches_array_type(const ArrayType* info, Type element_type, size_t count);
static ArrayType* create_array_type(Type element_type, size_t count);
static void format_array_type_name(ArrayType* info, const char* element_name, size_t length);

bool is_array_type(TypeKind kind)
{
    return kind == TypeArray;
}

size_t get_array_storage_size(Type type)
{
    if (!is_array_type(type.kind)) {
        switch (type.kind) {
            case TypeBool:
                return sizeof(uint8_t);
            case TypeChar:
            case TypeF32:
                return sizeof(uint32_t);
            case TypeF64:
                return sizeof(uint64_t);
            case TypeString:
                return sizeof(void*);
            default:
                return get_integer_type_size(type.kind);
        }
    }

    const ArrayType* info = type.array;

    return info->size;
}

Type get_array_leaf_type(Type type)
{
    if (!is_array_type(type.kind)) {
        return type;
    }

    const ArrayType* info = type.array;

    return get_array_leaf_type(info->element_type);
}

Type get_array_type(Vector* array_types, Type element_type, size_t count)
{
    ArrayType* info = find_array_type(array_types, element_type, count);

    if (info) {
        return (Type){.kind = TypeArray, .array = info};
    }

    info = create_array_type(element_type, count);

    push_vector_item(array_types, info);

    return (Type){.kind = TypeArray, .array = info};
}

static ArrayType* find_array_type(const Vector* array_types, Type element_type, size_t count)
{
    size_t index = 0;

    while (index < array_types->count && !matches_array_type(array_types->data[index], element_type, count)) {
        index++;
    }

    if (index == array_types->count) {
        return NULL;
    }

    return array_types->data[index];
}

static bool matches_array_type(const ArrayType* info, Type element_type, size_t count)
{
    return info->count == count && types_equal(info->element_type, element_type);
}

static ArrayType* create_array_type(Type element_type, size_t count)
{
    size_t element_size = get_array_storage_size(element_type);

    if (element_size && count > (SIZE_MAX - 128) / element_size) {
        fatal_error("Array size exceeds supported storage");
    }

    const char* element_name = get_data_type_name(element_type);
    size_t length = strlen(element_name) + 32;
    ArrayType* info = allocate_memory(sizeof(ArrayType) + length);
    info->element_type = element_type;
    info->dimensions = NULL;
    info->count = count;
    info->size = count * element_size;

    format_array_type_name(info, element_name, length);

    return info;
}

static void format_array_type_name(ArrayType* info, const char* element_name, size_t length)
{
    if (!is_array_type(info->element_type.kind)) {
        snprintf(info->name, length, "%s[%" PRIu64 "]", element_name, (uint64_t)info->count);

        return;
    }

    const char* dimensions = strchr(element_name, '[');
    int prefix_length = (int)(dimensions - element_name);

    snprintf(info->name, length, "%.*s[%" PRIu64 "]%s", prefix_length,
        element_name, (uint64_t)info->count, dimensions);
}
