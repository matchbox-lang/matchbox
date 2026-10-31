#include "builtin/system.h"
#include "common/errors.h"
#include "runtime/string_object.h"
#include "vm/vm.h"
#include <stdio.h>
#include <stdlib.h>

void builtinAssert(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    if (AS_BOOL(frame[0])) {
        return;
    }

    fatalError("Assertion failed");
}

void builtinAssertMessage(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    if (AS_BOOL(frame[0])) {
        return;
    }

    const StringObject* message = AS_STRING_OBJECT(frame[1]);

    fputs("Error: Assertion failed: ", stderr);
    fwrite(message->chars, 1, message->length, stderr);
    fputc('\n', stderr);
    raiseError();
}

void builtinByteorder(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    const int32_t i = 1;
    const char* c = (const char*)&i;
    
    frame[-2] = SIGNED_VALUE(*c == 0);
}

void builtinExit(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;
    (void)frame;

    exitVM(vm, EXIT_SUCCESS);
}

void builtinExitStatus(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;

    int status = AS_I32(frame[0]);

    exitVM(vm, status);
}

void builtinPanic(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    const StringObject* message = AS_STRING_OBJECT(frame[0]);

    fputs("Panic: ", stderr);
    fwrite(message->chars, 1, message->length, stderr);
    fputc('\n', stderr);
    raiseError();
}
