#include "vm/argument.h"
#include "runtime/concatenation.h"

typedef struct FormattedArguments
{
    Value* frame;
    const ArgumentFormat* formats;
    size_t count;
} FormattedArguments;

void formatArguments(Writer* writer, Value* frame, const ArgumentFormat* formats, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const ArgumentFormat* format = &formats[i];
        Value* argument = frame + format->slot;

        format->formatter(writer, argument);
    }
}

static void writeArgumentParts(Writer* writer, const void* context)
{
    const FormattedArguments* arguments = context;

    formatArguments(writer, arguments->frame, arguments->formats, arguments->count);
}

StringObject* concatenateArguments(Value* frame, const ArgumentFormat* formats, size_t count)
{
    FormattedArguments arguments = {frame, formats, count};

    return concatenateStringParts(writeArgumentParts, &arguments);
}

void formatI8Argument(Writer* writer, Value* argument)
{
    int32_t value = AS_I8(argument[0]);

    formatI32(writer, value);
}

void formatI16Argument(Writer* writer, Value* argument)
{
    int32_t value = AS_I16(argument[0]);

    formatI32(writer, value);
}

void formatI32Argument(Writer* writer, Value* argument)
{
    int32_t value = AS_I32(argument[0]);

    formatI32(writer, value);
}

void formatI64Argument(Writer* writer, Value* argument)
{
    int64_t value = readI64(argument, 0);

    formatI64(writer, value);
}

void formatU8Argument(Writer* writer, Value* argument)
{
    uint32_t value = AS_U8(argument[0]);

    formatU32(writer, value);
}

void formatU16Argument(Writer* writer, Value* argument)
{
    uint32_t value = AS_U16(argument[0]);

    formatU32(writer, value);
}

void formatU32Argument(Writer* writer, Value* argument)
{
    uint32_t value = AS_U32(argument[0]);

    formatU32(writer, value);
}

void formatU64Argument(Writer* writer, Value* argument)
{
    uint64_t value = readU64(argument, 0);

    formatU64(writer, value);
}

void formatF32Argument(Writer* writer, Value* argument)
{
    float value = AS_F32(argument[0]);

    formatF32(writer, value);
}

void formatF64Argument(Writer* writer, Value* argument)
{
    double value = readF64(argument, 0);

    formatF64(writer, value);
}

void formatBoolArgument(Writer* writer, Value* argument)
{
    bool value = AS_BOOL(argument[0]);

    formatBool(writer, value);
}

void formatCharArgument(Writer* writer, Value* argument)
{
    uint32_t value = AS_U32(argument[0]);

    formatChar(writer, value);
}

void formatStringArgument(Writer* writer, Value* argument)
{
    StringObject* value = AS_POINTER(argument[0]);

    formatString(writer, value);
}
