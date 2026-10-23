#include "builtin/string.h"
#include "runtime/string_concatenation.h"
#include "vm.h"

void builtinString(VM* vm, FunctionObject* function, Value* frame)
{
    if (function->argumentCount == 1 && function->argumentTypes[0] == TOKEN_STRING) {
        frame[-2] = frame[0];

        return;
    }

    StringObject* string = concatenateStringParts(frame, function->argumentTypes, function->argumentCount);

    pushVectorItem(&vm->module->objects, string);
    frame[-2] = POINTER_VALUE(string);
}
