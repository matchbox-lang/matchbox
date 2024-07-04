#include "value.h"
#include <stdlib.h>

ValueArray* initValueArray(ValueArray* array)
{
    array->capacity = ARRAY_INIT_CAPACITY;
    array->values = malloc(array->capacity * sizeof(Value));
    array->count = 0;

    return array;
}

void freeValueArray(ValueArray* array)
{
    free(array->values);
}

size_t countValueArray(ValueArray* array)
{
    return array->count;
}

void resizeValueArray(ValueArray* array, size_t capacity)
{
    array->values = realloc(array->values, sizeof(Value) * capacity);
    array->capacity = capacity;
}

void writeValueArray(ValueArray* array, Value value)
{
    if (array->capacity == array->count) {
        resizeValueArray(array, array->capacity * 2);
    }
    
    array->values[array->count++] = value;
}
