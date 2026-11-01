#include "runtime/value.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include <stdlib.h>
#include <string.h>

void init_value_array(ValueArray* array)
{
    array->data = NULL;
    array->capacity = 0;
    array->count = 0;
}

void free_value_array(ValueArray* array)
{
    free_memory(array->data);
}

int32_t read_signed_value(Value value, DataType type)
{
    if (type == TypeI8) {
        return AS_I8(value);
    }

    if (type == TypeI16) {
        return AS_I16(value);
    }

    return AS_I32(value);
}

uint32_t read_unsigned_value(Value value, DataType type)
{
    if (type == TypeU8) {
        return AS_U8(value);
    }

    if (type == TypeU16) {
        return AS_U16(value);
    }

    return AS_U32(value);
}

int64_t read_i64(const Value* frame, size_t position)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = AS_U32(frame[position]) | ((uint64_t)AS_U32(frame[position + 1]) << 32);

    return (int64_t)bits;
#else
    return AS_SIGNED(frame[position]);
#endif
}

uint64_t read_u64(const Value* frame, size_t position)
{
#if UINTPTR_MAX == UINT32_MAX
    return AS_U32(frame[position]) | ((uint64_t)AS_U32(frame[position + 1]) << 32);
#else
    return AS_UNSIGNED(frame[position]);
#endif
}

double read_f64(const Value* frame, size_t position)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = AS_U32(frame[position]) | ((uint64_t)AS_U32(frame[position + 1]) << 32);
    double value;
    memcpy(&value, &bits, sizeof(value));

    return value;
#else
    return AS_F64(frame[position]);
#endif
}

void write_i64(Value* frame, int64_t value)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = (uint64_t)value;

    frame[-2] = U32_VALUE(bits);
    frame[-1] = U32_VALUE(bits >> 32);
#else
    frame[-2] = SIGNED_VALUE(value);
#endif
}

void write_f64(Value* frame, double value)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits;

    memcpy(&bits, &value, sizeof(value));

    frame[-2] = U32_VALUE(bits);
    frame[-1] = U32_VALUE(bits >> 32);
#else
    frame[-2] = F64_VALUE(value);
#endif
}

size_t count_value_array(const ValueArray* array)
{
    return array->count;
}

void reserve_value_array(ValueArray* array, size_t capacity)
{
    if (capacity <= array->capacity) {
        return;
    }

    if (capacity > SIZE_MAX / sizeof(Value)) {
        out_of_memory_error();
    }

    Value* data = reallocate_memory(array->data, sizeof(Value) * capacity);
    array->data = data;
    array->capacity = capacity;
}

void resize_value_array(ValueArray* array, size_t size)
{
    if (array->capacity < size) {
        size_t capacity = MAX(array->capacity * 2, size);

        reserve_value_array(array, capacity);
    }

    for (size_t i = array->count; i < size; i++) {
        array->data[i] = SIGNED_VALUE(0);
    }

    array->count = size;
}

size_t push_value(ValueArray* array, Value value)
{
    if (array->count == array->capacity) {
        size_t capacity = MAX(array->capacity * 2, 4);

        reserve_value_array(array, capacity);
    }

    array->data[array->count++] = value;

    return array->count;
}

void* get_value_as_pointer(const ValueArray* array, size_t index)
{
    if (index >= array->count) {
        return NULL;
    }

    return AS_POINTER(array->data[index]);
}

void set_value_at(ValueArray* array, size_t index, Value item)
{
    if (index < array->count) {
        array->data[index] = item;
    }
}
