#include "builtin/io.h"
#include "runtime/argument_format.h"
#include "runtime/writer.h"
#include <stdio.h>

void builtinPrint(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    Writer writer;

    initOutputWriter(&writer, stdout);
    formatArguments(&writer, frame, function->argumentFormats, function->argumentCount);
    writeBytes(&writer, "\n", 1);
}
