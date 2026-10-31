#include "runtime/argument_format.h"
#include "common/errors.h"
#include "runtime/concatenation.h"
#include <stdint.h>

typedef struct FormattedArguments
{
    const Value* frame;
    const ArgumentFormat* formats;
    size_t count;
} FormattedArguments;

void formatArguments(Writer* writer, const Value* frame, const ArgumentFormat* formats, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const ArgumentFormat* format = &formats[i];
        const Value* argument = frame + format->slot;

        format->formatter(writer, argument);
    }
}

static void writeArgumentParts(Writer* writer, const void* context)
{
    const FormattedArguments* arguments = context;

    formatArguments(writer, arguments->frame, arguments->formats, arguments->count);
}

static size_t argumentCapacity(const Value* frame, const ArgumentFormat* format)
{
    if (format->capacity) {
        return format->capacity;
    }

    const StringObject* string = AS_POINTER(frame[format->slot]);

    return string->length;
}

static size_t addArgumentCapacity(size_t capacity, const Value* frame, const ArgumentFormat* format)
{
    size_t length = argumentCapacity(frame, format);

    if (length > SIZE_MAX - capacity) {
        outOfMemoryError();
    }

    return capacity + length;
}

static size_t getArgumentsCapacity(const Value* frame, const ArgumentFormat* formats, size_t count)
{
    size_t capacity = 0;

    for (size_t i = 0; i < count; i++) {
        capacity = addArgumentCapacity(capacity, frame, &formats[i]);
    }

    return capacity;
}

StringObject* concatenateArguments(const Value* frame, const ArgumentFormat* formats, size_t count)
{
    FormattedArguments arguments = {frame, formats, count};
    size_t capacity = getArgumentsCapacity(frame, formats, count);

    return concatenateStringParts(capacity, writeArgumentParts, &arguments);
}

void formatI8Argument(Writer* writer, const Value* argument)
{
    int32_t value = AS_I8(argument[0]);

    formatI32(writer, value);
}

void formatI16Argument(Writer* writer, const Value* argument)
{
    int32_t value = AS_I16(argument[0]);

    formatI32(writer, value);
}

void formatI32Argument(Writer* writer, const Value* argument)
{
    int32_t value = AS_I32(argument[0]);

    formatI32(writer, value);
}

void formatI64Argument(Writer* writer, const Value* argument)
{
    int64_t value = readI64(argument, 0);

    formatI64(writer, value);
}

void formatU8Argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U8(argument[0]);

    formatU32(writer, value);
}

void formatU16Argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U16(argument[0]);

    formatU32(writer, value);
}

void formatU32Argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U32(argument[0]);

    formatU32(writer, value);
}

void formatU64Argument(Writer* writer, const Value* argument)
{
    uint64_t value = readU64(argument, 0);

    formatU64(writer, value);
}

static void formatIsizeArgument(Writer* writer, const Value* argument)
{
    intptr_t value = AS_SIGNED(argument[0]);

    formatI64(writer, value);
}

static void formatUsizeArgument(Writer* writer, const Value* argument)
{
    uintptr_t value = AS_UNSIGNED(argument[0]);

    formatU64(writer, value);
}

void formatF32Argument(Writer* writer, const Value* argument)
{
    float value = AS_F32(argument[0]);

    formatF32(writer, value);
}

void formatF64Argument(Writer* writer, const Value* argument)
{
    double value = readF64(argument, 0);

    formatF64(writer, value);
}

void formatBoolArgument(Writer* writer, const Value* argument)
{
    bool value = AS_BOOL(argument[0]);

    formatBool(writer, value);
}

void formatCharArgument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U32(argument[0]);

    formatChar(writer, value);
}

void formatStringArgument(Writer* writer, const Value* argument)
{
    const StringObject* value = AS_POINTER(argument[0]);

    formatString(writer, value);
}

ArgumentFormat getArgumentFormat(DataType type)
{
    switch (type) {
        case TYPE_I8:
            return (ArgumentFormat){.formatter = formatI8Argument, .capacity = 4};
        case TYPE_I16:
            return (ArgumentFormat){.formatter = formatI16Argument, .capacity = 6};
        case TYPE_I32:
            return (ArgumentFormat){.formatter = formatI32Argument, .capacity = 11};
        case TYPE_I64:
            return (ArgumentFormat){.formatter = formatI64Argument, .capacity = 20};
        case TYPE_U8:
            return (ArgumentFormat){.formatter = formatU8Argument, .capacity = 3};
        case TYPE_U16:
            return (ArgumentFormat){.formatter = formatU16Argument, .capacity = 5};
        case TYPE_U32:
            return (ArgumentFormat){.formatter = formatU32Argument, .capacity = 10};
        case TYPE_U64:
            return (ArgumentFormat){.formatter = formatU64Argument, .capacity = 20};
        case TYPE_ISIZE:
            return (ArgumentFormat){.formatter = formatIsizeArgument, .capacity = 20};
        case TYPE_USIZE:
            return (ArgumentFormat){.formatter = formatUsizeArgument, .capacity = 20};
        case TYPE_F32:
            return (ArgumentFormat){.formatter = formatF32Argument, .capacity = 32};
        case TYPE_F64:
            return (ArgumentFormat){.formatter = formatF64Argument, .capacity = 32};
        case TYPE_CHAR:
            return (ArgumentFormat){.formatter = formatCharArgument, .capacity = 4};
        case TYPE_STRING:
            return (ArgumentFormat){.formatter = formatStringArgument};
        case TYPE_BOOL:
            return (ArgumentFormat){.formatter = formatBoolArgument, .capacity = 5};
        default:
            return (ArgumentFormat){0};
    }
}
