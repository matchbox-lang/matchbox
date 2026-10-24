#include "common/errors.h"
#include <stdio.h>
#include <stdlib.h>

void writeErrorMessage(const char* format, va_list arguments)
{
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, arguments);
}

void reportError(const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    writeErrorMessage(format, arguments);
    va_end(arguments);
    fprintf(stderr, "\n");
}

_Noreturn void fatalError(const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    writeErrorMessage(format, arguments);
    va_end(arguments);
    fprintf(stderr, "\n");
    exit(1);
}

_Noreturn void outOfMemoryError(void)
{
    fatalError("Out of memory");
}
