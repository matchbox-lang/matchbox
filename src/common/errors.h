#ifndef ERRORS_H
#define ERRORS_H

#include "common/memory.h"
#include <setjmp.h>
#include <stdarg.h>

typedef struct ErrorContext
{
    jmp_buf jump;
    struct ErrorContext* previous;
    MemoryGroup* previousMemory;
} ErrorContext;

void beginErrorContext(ErrorContext* context, MemoryGroup* memory);
void endErrorContext(ErrorContext* context);
_Noreturn void raiseError(void);

void writeErrorMessage(const char* format, va_list arguments);
void reportError(const char* format, ...);
_Noreturn void fatalError(const char* format, ...);
_Noreturn void outOfMemoryError(void);

#endif
