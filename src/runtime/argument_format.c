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

void format_arguments(Writer* writer, const Value* frame, const ArgumentFormat* formats, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const ArgumentFormat* format = &formats[i];
        const Value* argument = frame + format->slot;

        format->formatter(writer, argument);
    }
}

static void write_argument_parts(Writer* writer, const void* context)
{
    const FormattedArguments* arguments = context;

    format_arguments(writer, arguments->frame, arguments->formats, arguments->count);
}

static size_t argument_capacity(const Value* frame, const ArgumentFormat* format)
{
    if (format->capacity) {
        return format->capacity;
    }

    const StringObject* string = AS_POINTER(frame[format->slot]);

    return string->length;
}

static size_t add_argument_capacity(size_t capacity, const Value* frame, const ArgumentFormat* format)
{
    size_t length = argument_capacity(frame, format);

    if (length > SIZE_MAX - capacity) {
        out_of_memory_error();
    }

    return capacity + length;
}

static size_t get_arguments_capacity(const Value* frame, const ArgumentFormat* formats, size_t count)
{
    size_t capacity = 0;

    for (size_t i = 0; i < count; i++) {
        capacity = add_argument_capacity(capacity, frame, &formats[i]);
    }

    return capacity;
}

StringObject* concatenate_arguments(const Value* frame, const ArgumentFormat* formats, size_t count)
{
    FormattedArguments arguments = {frame, formats, count};
    size_t capacity = get_arguments_capacity(frame, formats, count);

    return concatenate_string_parts(capacity, write_argument_parts, &arguments);
}

void format_i8_argument(Writer* writer, const Value* argument)
{
    int32_t value = AS_I8(argument[0]);

    format_i32(writer, value);
}

void format_i16_argument(Writer* writer, const Value* argument)
{
    int32_t value = AS_I16(argument[0]);

    format_i32(writer, value);
}

void format_i32_argument(Writer* writer, const Value* argument)
{
    int32_t value = AS_I32(argument[0]);

    format_i32(writer, value);
}

void format_i64_argument(Writer* writer, const Value* argument)
{
    int64_t value = read_i64(argument, 0);

    format_i64(writer, value);
}

void format_u8_argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U8(argument[0]);

    format_u32(writer, value);
}

void format_u16_argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U16(argument[0]);

    format_u32(writer, value);
}

void format_u32_argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U32(argument[0]);

    format_u32(writer, value);
}

void format_u64_argument(Writer* writer, const Value* argument)
{
    uint64_t value = read_u64(argument, 0);

    format_u64(writer, value);
}

static void format_isize_argument(Writer* writer, const Value* argument)
{
    intptr_t value = AS_SIGNED(argument[0]);

    format_i64(writer, value);
}

static void format_usize_argument(Writer* writer, const Value* argument)
{
    uintptr_t value = AS_UNSIGNED(argument[0]);

    format_u64(writer, value);
}

void format_f32_argument(Writer* writer, const Value* argument)
{
    float value = AS_F32(argument[0]);

    format_f32(writer, value);
}

void format_f64_argument(Writer* writer, const Value* argument)
{
    double value = read_f64(argument, 0);

    format_f64(writer, value);
}

void format_bool_argument(Writer* writer, const Value* argument)
{
    bool value = AS_BOOL(argument[0]);

    format_bool(writer, value);
}

void format_char_argument(Writer* writer, const Value* argument)
{
    uint32_t value = AS_U32(argument[0]);

    format_char(writer, value);
}

void format_string_argument(Writer* writer, const Value* argument)
{
    const StringObject* value = AS_POINTER(argument[0]);

    format_string(writer, value);
}

ArgumentFormat get_argument_format(DataType type)
{
    switch (type) {
        case TypeI8:
            return (ArgumentFormat){.formatter = format_i8_argument, .capacity = 4};
        case TypeI16:
            return (ArgumentFormat){.formatter = format_i16_argument, .capacity = 6};
        case TypeI32:
            return (ArgumentFormat){.formatter = format_i32_argument, .capacity = 11};
        case TypeI64:
            return (ArgumentFormat){.formatter = format_i64_argument, .capacity = 20};
        case TypeU8:
            return (ArgumentFormat){.formatter = format_u8_argument, .capacity = 3};
        case TypeU16:
            return (ArgumentFormat){.formatter = format_u16_argument, .capacity = 5};
        case TypeU32:
            return (ArgumentFormat){.formatter = format_u32_argument, .capacity = 10};
        case TypeU64:
            return (ArgumentFormat){.formatter = format_u64_argument, .capacity = 20};
        case TypeIsize:
            return (ArgumentFormat){.formatter = format_isize_argument, .capacity = 20};
        case TypeUsize:
            return (ArgumentFormat){.formatter = format_usize_argument, .capacity = 20};
        case TypeF32:
            return (ArgumentFormat){.formatter = format_f32_argument, .capacity = 32};
        case TypeF64:
            return (ArgumentFormat){.formatter = format_f64_argument, .capacity = 32};
        case TypeChar:
            return (ArgumentFormat){.formatter = format_char_argument, .capacity = 4};
        case TypeString:
            return (ArgumentFormat){.formatter = format_string_argument};
        case TypeBool:
            return (ArgumentFormat){.formatter = format_bool_argument, .capacity = 5};
        default:
            return (ArgumentFormat){0};
    }
}
