#ifndef FUNCTION_H
#define FUNCTION_H

#include <stddef.h>
#include <stdint.h>

typedef struct Function
{
    int paramCount;
    int localCount;
    int maxStackCount;
    size_t position;
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
void reserveFunctionArray(FunctionArray* array, size_t capacity);
size_t pushFunction(FunctionArray* array, Function func);
Function getFunctionAt(FunctionArray* array, size_t index);
void setFunctionAt(FunctionArray* array, size_t index, Function func);

#endif
