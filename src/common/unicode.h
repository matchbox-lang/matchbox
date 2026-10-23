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

bool isUnicodeScalar(uint32_t value);
bool decodeUtf8(const char* chars, size_t length, DecodedScalar* scalar);
size_t encodeUtf8(uint32_t value, char bytes[4]);

#endif
