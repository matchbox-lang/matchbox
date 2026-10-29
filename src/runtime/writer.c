#include "runtime/writer.h"
#include "common/errors.h"
#include "common/float_format.h"
#include "common/unicode.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

void initBufferWriter(Writer* writer, char* bytes, size_t capacity)
{
    writer->bytes = bytes;
    writer->capacity = capacity;
    writer->length = 0;
    writer->stream = NULL;
}

void initOutputWriter(Writer* writer, FILE* stream)
{
    initBufferWriter(writer, NULL, 0);

    writer->stream = stream;
}

static void writeBufferBytes(Writer* writer, const char* bytes, size_t length)
{
    if (writer->length > writer->capacity || length > writer->capacity - writer->length) {
        fatalError("Formatting buffer capacity exceeded");
    }

    memcpy(writer->bytes + writer->length, bytes, length);
}

void writeBytes(Writer* writer, const char* bytes, size_t length)
{
    if (length > SIZE_MAX - writer->length) {
        outOfMemoryError();
    }

    if (writer->bytes) {
        writeBufferBytes(writer, bytes, length);
    }

    if (writer->stream) {
        fwrite(bytes, 1, length, writer->stream);
    }

    writer->length += length;
}

void formatI32(Writer* writer, int32_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRId32, value);

    writeBytes(writer, bytes, (size_t)length);
}

void formatI64(Writer* writer, int64_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRId64, value);

    writeBytes(writer, bytes, (size_t)length);
}

void formatU32(Writer* writer, uint32_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRIu32, value);

    writeBytes(writer, bytes, (size_t)length);
}

void formatU64(Writer* writer, uint64_t value)
{
    char bytes[32];
    int length = snprintf(bytes, sizeof(bytes), "%" PRIu64, value);

    writeBytes(writer, bytes, (size_t)length);
}

void formatF32(Writer* writer, float value)
{
    char bytes[32];

    formatShortestF32(bytes, sizeof(bytes), value);

    size_t length = strlen(bytes);

    writeBytes(writer, bytes, length);
}

void formatF64(Writer* writer, double value)
{
    char bytes[32];

    formatShortestF64(bytes, sizeof(bytes), value);

    size_t length = strlen(bytes);

    writeBytes(writer, bytes, length);
}

void formatBool(Writer* writer, bool value)
{
    if (value) {
        writeBytes(writer, "true", 4);

        return;
    }

    writeBytes(writer, "false", 5);
}

void formatChar(Writer* writer, uint32_t value)
{
    char bytes[4];
    size_t length = encodeUtf8(value, bytes);

    writeBytes(writer, bytes, length);
}

void formatString(Writer* writer, const StringObject* value)
{
    writeBytes(writer, value->chars, value->length);
}
