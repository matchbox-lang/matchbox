#include "function.h"
#include <stdlib.h>

void initFunctionArray(FunctionArray* array)
{
    array->capacity = ARRAY_INIT_CAPACITY;
    array->data = malloc(array->capacity * sizeof(Function));
    array->count = 0;
}

void freeFunctionArray(FunctionArray* array)
{
    free(array->data);
}

size_t countFunctionArray(FunctionArray* array)
{
    return array->count;
}

void resizeFunctionArray(FunctionArray* array, size_t capacity)
{
    array->data = realloc(array->data, sizeof(Function) * capacity);
    array->capacity = capacity;
}

void pushFunctionArray(FunctionArray* array, Function func)
{
    if (array->capacity == array->count) {
        resizeFunctionArray(array, array->capacity * 2);
    }
    
    array->data[array->count++] = func;
}

Function getFunctionAt(FunctionArray* array, size_t index)
{
    return array->data[index];
}

void setFunctionAt(FunctionArray* array, size_t index, Function item)
{
    if (index >= 0 && index < array->count) {
        array->data[index] = item;
    }
}
