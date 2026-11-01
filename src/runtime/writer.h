#ifndef WRITER_H
#define WRITER_H

#include "runtime/string_object.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Writer
{
    char* bytes;
    size_t capacity;
    size_t length;
    FILE* stream;
} Writer;

void init_buffer_writer(Writer* writer, char* bytes, size_t capacity);
void init_output_writer(Writer* writer, FILE* stream);
void write_bytes(Writer* writer, const char* bytes, size_t length);
void format_i32(Writer* writer, int32_t value);
void format_i64(Writer* writer, int64_t value);
void format_u32(Writer* writer, uint32_t value);
void format_u64(Writer* writer, uint64_t value);
void format_f32(Writer* writer, float value);
void format_f64(Writer* writer, double value);
void format_bool(Writer* writer, bool value);
void format_char(Writer* writer, uint32_t value);
void format_string(Writer* writer, const StringObject* value);

#endif
