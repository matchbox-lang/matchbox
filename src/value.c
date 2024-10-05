#include "value.h"
#include <stdlib.h>

void initValueArray(ValueArray* array)
{
    array->capacity = ARRAY_INIT_CAPACITY;
    array->data = malloc(array->capacity * sizeof(Value));
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

void resizeValueArray(ValueArray* array, size_t capacity)
{
    array->data = realloc(array->data, sizeof(Value) * capacity);
    array->capacity = capacity;
}

void pushValueArray(ValueArray* array, Value value)
{
    if (array->capacity == array->count) {
        resizeValueArray(array, array->capacity * 2);
    }
    
    array->data[array->count++] = value;
}

Value getValueAt(ValueArray* array, size_t index)
{
    return array->data[index];
}

void setValueAt(ValueArray* array, size_t index, Value item)
{
    if (index >= 0 && index < array->count) {
        array->data[index] = item;
    }
}
