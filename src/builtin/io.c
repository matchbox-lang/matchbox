#include "builtin/io.h"
#include "common/errors.h"
#include "common/io.h"
#include "common/memory.h"
#include "runtime/argument_format.h"
#include "runtime/writer.h"
#include "vm/vm.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static _Noreturn void read_input_error(int error_number)
{
    const char* detail = strerror(error_number);

    fatal_error("Could not read standard input: %s", detail);
}

static size_t input_line_length(const char* line, int count)
{
    if (count <= 0) {
        return 0;
    }

    size_t length = (size_t)count;

    if (line[length - 1] != '\n') {
        return length;
    }

    length--;

    if (length > 0 && line[length - 1] == '\r') {
        length--;
    }

    return length;
}

void builtin_input(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)function;

    char* line = NULL;
    size_t capacity = 0;

    fflush(stdout);

    int count = read_stream_line(&line, &capacity, stdin);
    int error_number = errno;

    if (ferror(stdin)) {
        free_memory(line);
        read_input_error(error_number);
    }

    size_t length = input_line_length(line, count);
    StringObject* string = copy_string_object(line, length);

    free_memory(line);
    push_vector_item(&vm->module->objects, string);

    frame[-2] = POINTER_VALUE(string);
}

void builtin_print(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    Writer writer;

    init_output_writer(&writer, stdout);
    format_arguments(&writer, frame, function->argument_formats, function->argument_count);
    write_bytes(&writer, "\n", 1);
}
