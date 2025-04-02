#include "value.h"
#include <stdlib.h>
#include <stdio.h>

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity * 2)

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

size_t countValueArray(ValueArray* array)
{
    return array->count;
}

void reserveValueArray(ValueArray* array, size_t capacity)
{
    if (capacity > array->capacity) {
        array->data = realloc(array->data, sizeof(Value) * capacity);
        array->capacity = capacity;
    }
}

void resizeValueArray(ValueArray* array, size_t size)
{
    reserveValueArray(array, size);
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
    if (index < 0 || index >= array->count) {
        return NULL;
    }

    return AS_POINTER(array->data[index]);
}

void setValueAt(ValueArray* array, size_t index, Value item)
{
    if (index >= 0 && index < array->count) {
        array->data[index] = item;
    }
}
