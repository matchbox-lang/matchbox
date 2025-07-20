#include "builtin.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

Value __exit(int argCount, Value* args)
{
    exit(0);
}

Value __print(int argCount, Value* args)
{
    int32_t n = AS_INT(args[0]);
    
    printf("%d\n", n);

    return INT_VALUE(0);
}

Value __clamp(int argCount, Value* args)
{
    int32_t num = AS_INT(args[0]);
    int32_t min = AS_INT(args[1]);
    int32_t max = AS_INT(args[2]);

    if (num < min) {
        return INT_VALUE(min);
    } else if (num > max) {
        return INT_VALUE(max);
    } else {
        return INT_VALUE(num);
    }
}

Value __abs(int argCount, Value* args)
{
    int32_t n = AS_INT(args[0]);
    int32_t x = n < 0 ? -n : n;
    
    return INT_VALUE(x);
}

Value __min(int argCount, Value* args)
{
    int32_t a = AS_INT(args[0]);
    int32_t b = AS_INT(args[1]);
    int32_t x = a < b ? a : b;
    
    return INT_VALUE(x);
}

Value __max(int argCount, Value* args)
{
    int32_t a = AS_INT(args[0]);
    int32_t b = AS_INT(args[1]);
    int32_t x = a > b ? a : b;
    
    return INT_VALUE(x);
}

Value __byteorder(int argCount, Value* args)
{
    int32_t i = 1;
    char* c = (char*)&i;
    int32_t x = (int32_t)*c;
    
    return INT_VALUE(x);
}
