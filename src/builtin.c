#include "builtin.h"
#include "token.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Builtin builtins[BUILTINS_MAX] = {
    [BUILTIN_EXIT] = {"exit", BUILTIN_EXIT, 0, {}, TOKEN_NONE},
    [BUILTIN_PRINT] = {"print", BUILTIN_PRINT, 1, {TOKEN_INT}, TOKEN_NONE},
    [BUILTIN_CLAMP] = {"clamp", BUILTIN_CLAMP, 3, {TOKEN_INT, TOKEN_INT, TOKEN_INT}, TOKEN_INT},
    [BUILTIN_ABS] = {"abs", BUILTIN_ABS, 1, {TOKEN_INT}, TOKEN_INT},
    [BUILTIN_MIN] = {"min", BUILTIN_MIN, 2, {TOKEN_INT, TOKEN_INT}, TOKEN_INT},
    [BUILTIN_MAX] = {"max", BUILTIN_MAX, 2, {TOKEN_INT, TOKEN_INT}, TOKEN_INT},
    [BUILTIN_BYTEORDER] = {"byteorder", BUILTIN_BYTEORDER, 0, {}, TOKEN_INT}
};

Builtin* getBuiltinByName(const char* name)
{
    for (int i = 0; i < BUILTINS_MAX; i++) {
        Builtin* builtin = &builtins[i];

        if (strcmp(name, builtin->name) == 0) {
            return builtin;
        }
    }

    return NULL;
}

Value builtinExit(Value* args)
{
    exit(0);
}

Value builtinPrint(Value* args)
{
    int32_t n = AS_INT(args[0]);
    
    printf("%d\n", n);

    return INT_VALUE(0);
}

Value builtinClamp(Value* args)
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

Value builtinAbs(Value* args)
{
    int32_t n = AS_INT(args[0]);
    int32_t x = n < 0 ? -n : n;
    
    return INT_VALUE(x);
}

Value builtinMin(Value* args)
{
    int32_t a = AS_INT(args[0]);
    int32_t b = AS_INT(args[1]);
    int32_t x = a < b ? a : b;
    
    return INT_VALUE(x);
}

Value builtinMax(Value* args)
{
    int32_t a = AS_INT(args[0]);
    int32_t b = AS_INT(args[1]);
    int32_t x = a > b ? a : b;
    
    return INT_VALUE(x);
}

Value builtinByteorder(Value* args)
{
    int32_t i = 1;
    char* c = (char*)&i;
    
    return INT_VALUE(*c == 0);
}
