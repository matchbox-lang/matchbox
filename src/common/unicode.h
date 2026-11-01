#ifndef UNICODE_H
#define UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct DecodedScalar
{
    uint32_t value;
    size_t length;
} DecodedScalar;

bool is_unicode_scalar(uint32_t value);
bool decode_utf8(const char* chars, size_t length, DecodedScalar* scalar);
size_t encode_utf8(uint32_t value, char bytes[4]);

#endif
