#include "builtin/system.h"
#include "common/errors.h"
#include "runtime/string_object.h"
#include "vm/vm.h"
#include <stdio.h>
#include <stdlib.h>

void builtin_assert(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    if (AS_BOOL(frame[0])) {
        return;
    }

    fatal_error("Assertion failed");
}

void builtin_assert_message(VM* vm, const FunctionObject* function, Value* frame)
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
    raise_error();
}

void builtin_byteorder(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    const int32_t i = 1;
    const char* c = (const char*)&i;
    
    frame[-2] = SIGNED_VALUE(*c == 0);
}

void builtin_exit(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;
    (void)frame;

    exit_vm(vm, EXIT_SUCCESS);
}

void builtin_exit_status(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;

    int status = AS_I32(frame[0]);

    exit_vm(vm, status);
}

void builtin_panic(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    const StringObject* message = AS_STRING_OBJECT(frame[0]);

    fputs("Panic: ", stderr);
    fwrite(message->chars, 1, message->length, stderr);
    fputc('\n', stderr);
    raise_error();
}
