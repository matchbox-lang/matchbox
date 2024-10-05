#ifndef FUNCTION_H
#define FUNCTION_H

#include <stddef.h>
#include <stdint.h>

#define ARRAY_INIT_CAPACITY 4

typedef struct Function
{
    int paramsCount;
    uint8_t* bytecode;
} Function;

typedef struct FunctionArray
{
    Function* data;
    size_t capacity;
    size_t count;
} FunctionArray;

void initFunctionArray(FunctionArray* array);
void freeFunctionArray(FunctionArray* array);
size_t countFunctionArray(FunctionArray* array);
void resizeFunctionArray(FunctionArray* array, size_t capacity);
void pushFunctionArray(FunctionArray* array, Function func);

#endif
