#include "value.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

void initValueArray(ValueArray* array)
{
    array->data = NULL;
    array->capacity = 0;
    array->count = 0;
}

void freeValueArray(ValueArray* array)
{
    free(array->data);
}

int64_t readI64(Value* frame, size_t position)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = AS_U32(frame[position]) | ((uint64_t)AS_U32(frame[position + 1]) << 32);

    return (int64_t)bits;
#else
    return AS_SIGNED(frame[position]);
#endif
}

uint64_t readU64(Value* frame, size_t position)
{
#if UINTPTR_MAX == UINT32_MAX
    return AS_U32(frame[position]) | ((uint64_t)AS_U32(frame[position + 1]) << 32);
#else
    return AS_UNSIGNED(frame[position]);
#endif
}

double readF64(Value* frame, size_t position)
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

void writeF64(Value* frame, size_t position, double value)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    frame[position] = U32_VALUE(bits);
    frame[position + 1] = U32_VALUE(bits >> 32);
#else
    frame[position] = F64_VALUE(value);
#endif
}

void writeI64(Value* frame, int64_t value)
{
#if UINTPTR_MAX == UINT32_MAX
    uint64_t bits = (uint64_t)value;

    frame[-2] = U32_VALUE(bits);
    frame[-1] = U32_VALUE(bits >> 32);
#else
    frame[-2] = SIGNED_VALUE(value);
#endif
}

size_t countValueArray(ValueArray* array)
{
    return array->count;
}

void reserveValueArray(ValueArray* array, size_t capacity)
{
    if (capacity <= array->capacity) {
        return;
    }

    Value* data = realloc(array->data, sizeof(Value) * capacity);
    if (!data) {
        outOfMemoryError();
    }

    array->data = data;
    array->capacity = capacity;
}

void resizeValueArray(ValueArray* array, size_t size)
{
    reserveValueArray(array, size);

    for (size_t i = array->count; i < size; i++) {
        array->data[i] = SIGNED_VALUE(0);
    }

    array->count = size;
}

size_t pushValue(ValueArray* array, Value value)
{
    if (array->count == array->capacity) {
        reserveValueArray(array, GROW_CAPACITY(array->capacity));
    }

    array->data[array->count++] = value;
    return array->count;
}

void* getValueAsPointer(ValueArray* array, size_t index)
{
    if (index >= array->count) {
        return NULL;
    }

    return AS_POINTER(array->data[index]);
}

void setValueAt(ValueArray* array, size_t index, Value item)
{
    if (index < array->count) {
        array->data[index] = item;
    }
}
