#include "builtin/io.h"
#include "common/float_format.h"
#include "runtime/output.h"
#include "runtime/string_object.h"
#include "common/unicode.h"
#include <inttypes.h>
#include <stdio.h>

void builtinPrint(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    printValues(frame, function->argumentTypes, function->argumentCount);
    putchar('\n');
}

void builtinPrintBool(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    printf("%s\n", AS_BOOL(frame[0]) ? "true" : "false");
}

void builtinPrintChar(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    uint32_t value = AS_U32(frame[0]);
    char bytes[4];
    size_t length = encodeUtf8(value, bytes);

    fwrite(bytes, 1, length, stdout);
    putchar('\n');
}

void builtinPrintF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float value = AS_F32(frame[0]);
    char buffer[32];

    formatShortestF32(buffer, sizeof(buffer), value);
    printf("%s\n", buffer);
}

void builtinPrintF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double value = readF64(frame, 0);
    char buffer[32];

    formatShortestF64(buffer, sizeof(buffer), value);
    printf("%s\n", buffer);
}

void builtinPrintI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_I32(frame[0]);
    
    printf("%" PRId32 "\n", n);
}

void builtinPrintI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t n = readI64(frame, 0);

    printf("%" PRId64 "\n", n);
}

void builtinPrintString(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    StringObject* string = AS_POINTER(frame[0]);

    printStringObject(string);
    putchar('\n');
}

void builtinPrintU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    uint32_t n = AS_U32(frame[0]);

    printf("%" PRIu32 "\n", n);
}

void builtinPrintU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    uint64_t n = readU64(frame, 0);

    printf("%" PRIu64 "\n", n);
}
