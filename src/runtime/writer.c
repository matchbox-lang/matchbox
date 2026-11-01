#include "runtime/writer.h"
#include "common/errors.h"
#include "common/float_format.h"
#include "common/unicode.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

void init_buffer_writer(Writer* writer, char* bytes, size_t capacity)
{
    writer->bytes = bytes;
    writer->capacity = capacity;
    writer->length = 0;
    writer->stream = NULL;
}

void init_output_writer(Writer* writer, FILE* stream)
{
    init_buffer_writer(writer, NULL, 0);

    writer->stream = stream;
}

static void write_buffer_bytes(Writer* writer, const char* bytes, size_t length)
{
    if (writer->length > writer->capacity || length > writer->capacity - writer->length) {
        fatal_error("Formatting buffer capacity exceeded");
    }

    memcpy(writer->bytes + writer->length, bytes, length);
}

void write_bytes(Writer* writer, const char* bytes, size_t length)
{
    if (length > SIZE_MAX - writer->length) {
        out_of_memory_error();
    }

    if (writer->bytes) {
        write_buffer_bytes(writer, bytes, length);
    }

    if (writer->stream) {
        fwrite(bytes, 1, length, writer->stream);
    }

    writer->length += length;
}

void format_i32(Writer* writer, int32_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRId32, value);

    write_bytes(writer, bytes, (size_t)length);
}

void format_i64(Writer* writer, int64_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRId64, value);

    write_bytes(writer, bytes, (size_t)length);
}

void format_u32(Writer* writer, uint32_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRIu32, value);

    write_bytes(writer, bytes, (size_t)length);
}

void format_u64(Writer* writer, uint64_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRIu64, value);

    write_bytes(writer, bytes, (size_t)length);
}

void format_f32(Writer* writer, float value)
{
    char bytes[32];

    format_shortest_f32(bytes, sizeof(bytes), value);

    size_t length = strlen(bytes);

    write_bytes(writer, bytes, length);
}

void format_f64(Writer* writer, double value)
{
    char bytes[32];

    format_shortest_f64(bytes, sizeof(bytes), value);

    size_t length = strlen(bytes);

    write_bytes(writer, bytes, length);
}

void format_bool(Writer* writer, bool value)
{
    if (value) {
        write_bytes(writer, "true", 4);

        return;
    }

    write_bytes(writer, "false", 5);
}

void format_char(Writer* writer, uint32_t value)
{
    char bytes[4];
    size_t length = encode_utf8(value, bytes);

    write_bytes(writer, bytes, length);
}

void format_string(Writer* writer, const StringObject* value)
{
    write_bytes(writer, value->chars, value->length);
}
