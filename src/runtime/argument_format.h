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

ArgumentFormat get_argument_format(DataType type);
void format_arguments(Writer* writer, const Value* frame, const ArgumentFormat* formats, size_t count);
StringObject* concatenate_arguments(const Value* frame, const ArgumentFormat* formats, size_t count);
void format_i8_argument(Writer* writer, const Value* argument);
void format_i16_argument(Writer* writer, const Value* argument);
void format_i32_argument(Writer* writer, const Value* argument);
void format_i64_argument(Writer* writer, const Value* argument);
void format_u8_argument(Writer* writer, const Value* argument);
void format_u16_argument(Writer* writer, const Value* argument);
void format_u32_argument(Writer* writer, const Value* argument);
void format_u64_argument(Writer* writer, const Value* argument);
void format_f32_argument(Writer* writer, const Value* argument);
void format_f64_argument(Writer* writer, const Value* argument);
void format_bool_argument(Writer* writer, const Value* argument);
void format_char_argument(Writer* writer, const Value* argument);
void format_string_argument(Writer* writer, const Value* argument);

#endif
