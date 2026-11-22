#include "builtin/string.h"
#include "common/errors.h"
#include "common/unicode.h"
#include "runtime/argument_format.h"
#include "vm/vm.h"

void builtin_string(VM* vm, const FunctionObject* function, Value* frame)
{
    StringObject* string = concatenate_arguments(frame, function->argument_formats, function->argument_count);

    push_vector_item(&vm->module->objects, string);
    frame[-2] = POINTER_VALUE(string);
}

static void validate_string_boundary(const char* data, size_t index, size_t length)
{
    bool valid = is_utf8_boundary(data, index, length);

    if (!valid) {
        fatal_error("String slice bound splits a UTF-8 character");
    }
}

void builtin_validate_string_slice(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    const char* data = AS_POINTER(frame[0]);
    size_t start = AS_UNSIGNED(frame[1]);
    size_t end = AS_UNSIGNED(frame[2]);
    size_t length = AS_UNSIGNED(frame[3]);

    validate_string_boundary(data, start, length);
    validate_string_boundary(data, end, length);
}
