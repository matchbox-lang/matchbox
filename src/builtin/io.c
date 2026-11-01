#include "builtin/io.h"
#include "runtime/argument_format.h"
#include "runtime/writer.h"
#include <stdio.h>

void builtin_print(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    Writer writer;

    init_output_writer(&writer, stdout);
    format_arguments(&writer, frame, function->argument_formats, function->argument_count);
    write_bytes(&writer, "\n", 1);
}
