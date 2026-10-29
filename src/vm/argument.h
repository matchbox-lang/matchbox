#ifndef ARGUMENT_H
#define ARGUMENT_H

#include "runtime/writer.h"
#include "runtime/value.h"

typedef void (*ArgumentFormatter)(Writer* writer, Value* argument);

typedef struct ArgumentFormat
{
    ArgumentFormatter formatter;
    size_t slot;
    size_t capacity;
} ArgumentFormat;

void formatArguments(Writer* writer, Value* frame, const ArgumentFormat* formats, size_t count);
StringObject* concatenateArguments(Value* frame, const ArgumentFormat* formats, size_t count);
void formatI8Argument(Writer* writer, Value* argument);
void formatI16Argument(Writer* writer, Value* argument);
void formatI32Argument(Writer* writer, Value* argument);
void formatI64Argument(Writer* writer, Value* argument);
void formatU8Argument(Writer* writer, Value* argument);
void formatU16Argument(Writer* writer, Value* argument);
void formatU32Argument(Writer* writer, Value* argument);
void formatU64Argument(Writer* writer, Value* argument);
void formatF32Argument(Writer* writer, Value* argument);
void formatF64Argument(Writer* writer, Value* argument);
void formatBoolArgument(Writer* writer, Value* argument);
void formatCharArgument(Writer* writer, Value* argument);
void formatStringArgument(Writer* writer, Value* argument);

#endif
