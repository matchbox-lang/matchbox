#ifndef ARGUMENT_FORMAT_H
#define ARGUMENT_FORMAT_H

#include "runtime/value.h"
#include "runtime/writer.h"

typedef void (*ArgumentFormatter)(Writer* writer, const Value* argument);

typedef struct ArgumentFormat
{
    ArgumentFormatter formatter;
    size_t slot;
    size_t capacity;
} ArgumentFormat;

ArgumentFormat getArgumentFormat(DataType type);
void formatArguments(Writer* writer, const Value* frame, const ArgumentFormat* formats, size_t count);
StringObject* concatenateArguments(const Value* frame, const ArgumentFormat* formats, size_t count);
void formatI8Argument(Writer* writer, const Value* argument);
void formatI16Argument(Writer* writer, const Value* argument);
void formatI32Argument(Writer* writer, const Value* argument);
void formatI64Argument(Writer* writer, const Value* argument);
void formatU8Argument(Writer* writer, const Value* argument);
void formatU16Argument(Writer* writer, const Value* argument);
void formatU32Argument(Writer* writer, const Value* argument);
void formatU64Argument(Writer* writer, const Value* argument);
void formatF32Argument(Writer* writer, const Value* argument);
void formatF64Argument(Writer* writer, const Value* argument);
void formatBoolArgument(Writer* writer, const Value* argument);
void formatCharArgument(Writer* writer, const Value* argument);
void formatStringArgument(Writer* writer, const Value* argument);

#endif
