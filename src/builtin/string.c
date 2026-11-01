#include "builtin/string.h"
#include "runtime/argument_format.h"
#include "vm/vm.h"

void builtin_string(VM* vm, const FunctionObject* function, Value* frame)
{
    StringObject* string = concatenate_arguments(frame, function->argument_formats, function->argument_count);

    push_vector_item(&vm->module->objects, string);
    frame[-2] = POINTER_VALUE(string);
}
