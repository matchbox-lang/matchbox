#include "common/errors.h"
#include <stdio.h>
#include <stdlib.h>

static _Thread_local ErrorContext* currentContext;

_Noreturn void raiseError(void)
{
    if (currentContext) {
        longjmp(currentContext->jump, 1);
    }

    exit(1);
}

void beginErrorContext(ErrorContext* context, MemoryGroup* memory)
{
    context->previous = currentContext;
    context->previousMemory = useMemoryGroup(memory);

    if (!memory) {
        useMemoryGroup(context->previousMemory);
    }

    currentContext = context;
}

void endErrorContext(ErrorContext* context)
{
    currentContext = context->previous;

    useMemoryGroup(context->previousMemory);
}

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
    raiseError();
}

_Noreturn void outOfMemoryError(void)
{
    fatalError("Out of memory");
}
