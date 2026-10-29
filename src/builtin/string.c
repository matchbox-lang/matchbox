#include "builtin/string.h"
#include "vm/argument.h"
#include "vm/vm.h"

void builtinString(VM* vm, FunctionObject* function, Value* frame)
{
    StringObject* string = concatenateArguments(frame, function->argumentFormats, function->argumentCount);

    pushVectorItem(&vm->module->objects, string);
    frame[-2] = POINTER_VALUE(string);
}
