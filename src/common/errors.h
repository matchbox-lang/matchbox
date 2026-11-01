#ifndef ERRORS_H
#define ERRORS_H

#include "common/memory.h"
#include <setjmp.h>
#include <stdarg.h>

typedef struct ErrorContext
{
    jmp_buf jump;
    struct ErrorContext* previous;
    MemoryGroup* previous_memory;
} ErrorContext;

void begin_error_context(ErrorContext* context, MemoryGroup* memory);
void end_error_context(ErrorContext* context);
_Noreturn void raise_error(void);

void write_error_message(const char* format, va_list arguments);
void report_error(const char* format, ...);
_Noreturn void fatal_error(const char* format, ...);
_Noreturn void out_of_memory_error(void);

#endif
