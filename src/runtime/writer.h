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

void initBufferWriter(Writer* writer, char* bytes, size_t capacity);
void initOutputWriter(Writer* writer, FILE* stream);
void writeBytes(Writer* writer, const char* bytes, size_t length);
void formatI32(Writer* writer, int32_t value);
void formatI64(Writer* writer, int64_t value);
void formatU32(Writer* writer, uint32_t value);
void formatU64(Writer* writer, uint64_t value);
void formatF32(Writer* writer, float value);
void formatF64(Writer* writer, double value);
void formatBool(Writer* writer, bool value);
void formatChar(Writer* writer, uint32_t value);
void formatString(Writer* writer, const StringObject* value);

#endif
