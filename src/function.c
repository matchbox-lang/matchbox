#include "function.h"
#include <stdlib.h>

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity * 2)

void initFunctionArray(FunctionArray* array)
{
    array->data = NULL;
    array->capacity = 0;
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

void reserveFunctionArray(FunctionArray* array, size_t capacity)
{
    array->data = realloc(array->data, sizeof(Function) * capacity);
    array->capacity = capacity;
}

size_t pushFunction(FunctionArray* array, Function func)
{
    if (array->capacity == array->count) {
        reserveFunctionArray(array, GROW_CAPACITY(array->capacity));
    }
    
    array->data[array->count++] = func;
    return array->count;
}

Function getFunctionAt(FunctionArray* array, size_t index)
{
    return array->data[index];
}

void setFunctionAt(FunctionArray* array, size_t index, Function func)
{
    if (index >= 0 && index < array->count) {
        array->data[index] = func;
    }
}
