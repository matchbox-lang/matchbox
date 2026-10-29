#include "builtin/string.h"
#include "vm/argument.h"
#include "vm/vm.h"

void builtinString(VM* vm, FunctionObject* function, Value* frame)
{
    if (function->argumentCount == 1 && function->argumentFormats[0].formatter == formatStringArgument) {
        frame[-2] = frame[0];

        return;
    }

    StringObject* string = concatenateArguments(frame, function->argumentFormats, function->argumentCount);

    pushVectorItem(&vm->module->objects, string);
    frame[-2] = POINTER_VALUE(string);
}
