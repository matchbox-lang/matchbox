#include "common/array_type.h"
#include "common/errors.h"
#include "common/memory.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static ArrayType* find_array_type(ArrayType* info, DataType element_type, size_t count);
static ArrayType* create_array_type(DataType element_type, size_t count);
static void format_array_type_name(ArrayType* info, const char* element_name, size_t length);

bool is_array_type(DataType type)
{
    return type > TypeArray;
}

const ArrayType* get_array_type_info(DataType type)
{
    return (const ArrayType*)type;
}

size_t get_array_storage_slots(DataType type)
{
    if (!is_array_type(type)) {
        return get_type_slot_count(type);
    }

    const ArrayType* info = get_array_type_info(type);

    return info->slots;
}

DataType get_array_leaf_type(DataType type)
{
    if (!is_array_type(type)) {
        return type;
    }

    const ArrayType* info = get_array_type_info(type);

    return get_array_leaf_type(info->element_type);
}

DataType get_array_type(DataType element_type, size_t count)
{
    void** registry = get_array_type_registry();
    ArrayType* info = find_array_type(*registry, element_type, count);

    if (info) {
        return (DataType)info;
    }

    info = create_array_type(element_type, count);
    info->next = *registry;
    *registry = info;

    return (DataType)info;
}

static ArrayType* find_array_type(ArrayType* info, DataType element_type, size_t count)
{
    while (info && (info->element_type != element_type || info->count != count)) {
        info = info->next;
    }

    return info;
}

static ArrayType* create_array_type(DataType element_type, size_t count)
{
    size_t element_slots = get_array_storage_slots(element_type);

    if (element_slots && count > (SIZE_MAX - 128) / sizeof(void*) / element_slots) {
        fatal_error("Array size exceeds supported storage");
    }

    const char* element_name = get_data_type_name(element_type);
    size_t length = strlen(element_name) + 32;
    ArrayType* info = allocate_memory(sizeof(ArrayType) + length);
    info->element_type = element_type;
    info->count = count;
    info->slots = count * element_slots;

    format_array_type_name(info, element_name, length);

    return info;
}

static void format_array_type_name(ArrayType* info, const char* element_name, size_t length)
{
    if (!is_array_type(info->element_type)) {
        snprintf(info->name, length, "%s[%" PRIu64 "]", element_name, (uint64_t)info->count);

        return;
    }

    const char* dimensions = strchr(element_name, '[');
    int prefix_length = (int)(dimensions - element_name);

    snprintf(info->name, length, "%.*s[%" PRIu64 "]%s", prefix_length,
        element_name, (uint64_t)info->count, dimensions);
}
