#include "builtin.h"
#include "token.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Builtin builtins[BUILTINS_MAX] = {
    {"exit",        BUILTIN_EXIT,       0, {},                                  TOKEN_VOID},
    {"print",       BUILTIN_PRINT,      1, {TOKEN_INT},                         TOKEN_VOID},
    {"clamp",       BUILTIN_CLAMP,      3, {TOKEN_INT, TOKEN_INT, TOKEN_INT},   TOKEN_INT},
    {"abs",         BUILTIN_ABS,        1, {TOKEN_INT},                         TOKEN_INT},
    {"min",         BUILTIN_MIN,        2, {TOKEN_INT, TOKEN_INT},              TOKEN_INT},
    {"max",         BUILTIN_MAX,        2, {TOKEN_INT, TOKEN_INT},              TOKEN_INT},
    {"byteorder",   BUILTIN_BYTEORDER,  0, {},                                  TOKEN_INT}
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

void builtinExit(Value* frame)
{
    (void)frame;
    exit(0);
}

void builtinPrint(Value* frame)
{
    int32_t n = AS_INT(frame[0]);
    
    printf("%d\n", n);
}

void builtinClamp(Value* frame)
{
    int32_t num = AS_INT(frame[0]);
    int32_t min = AS_INT(frame[1]);
    int32_t max = AS_INT(frame[2]);

    if (num < min) {
        num = min;
    } else if (num > max) {
        num = max;
    }

    frame[0] = INT_VALUE(num);
}

void builtinAbs(Value* frame)
{
    int32_t n = AS_INT(frame[0]);
    int32_t x = n < 0 ? -n : n;
    
    frame[0] = INT_VALUE(x);
}

void builtinMin(Value* frame)
{
    int32_t a = AS_INT(frame[0]);
    int32_t b = AS_INT(frame[1]);
    int32_t x = a < b ? a : b;
    
    frame[0] = INT_VALUE(x);
}

void builtinMax(Value* frame)
{
    int32_t a = AS_INT(frame[0]);
    int32_t b = AS_INT(frame[1]);
    int32_t x = a > b ? a : b;
    
    frame[0] = INT_VALUE(x);
}

void builtinByteorder(Value* frame)
{
    int32_t i = 1;
    char* c = (char*)&i;
    
    frame[0] = INT_VALUE(*c == 0);
}
