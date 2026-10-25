#include "builtin/system.h"
#include <stdlib.h>

void builtinByteorder(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t i = 1;
    char* c = (char*)&i;
    
    frame[-2] = SIGNED_VALUE(*c == 0);
}

void builtinExit(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;
    (void)frame;

    exit(EXIT_SUCCESS);
}

void builtinExitStatus(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t status = AS_I32(frame[0]);

    exit(status);
}
