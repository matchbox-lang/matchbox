#include "common/errors.h"
#include <stdio.h>
#include <stdlib.h>

static _Thread_local ErrorContext* current_context;

_Noreturn void raise_error(void)
{
    if (current_context) {
        longjmp(current_context->jump, 1);
    }

    exit(EXIT_FAILURE);
}

void begin_error_context(ErrorContext* context, MemoryGroup* memory)
{
    context->previous = current_context;
    context->previous_memory = use_memory_group(memory);

    if (!memory) {
        use_memory_group(context->previous_memory);
    }

    current_context = context;
}

void end_error_context(ErrorContext* context)
{
    current_context = context->previous;

    use_memory_group(context->previous_memory);
}

void write_error_message(const char* format, va_list arguments)
{
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, arguments);
}

void report_error(const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    write_error_message(format, arguments);
    va_end(arguments);
    fprintf(stderr, "\n");
}

_Noreturn void fatal_error(const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    write_error_message(format, arguments);
    va_end(arguments);
    fprintf(stderr, "\n");
    raise_error();
}

_Noreturn void out_of_memory_error(void)
{
    fatal_error("Out of memory");
}
