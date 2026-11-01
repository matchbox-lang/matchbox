#include "common/unicode.h"

bool is_unicode_scalar(uint32_t value)
{
    return value <= 0x10FFFF && (value < 0xD800 || value > 0xDFFF);
}

static size_t utf8_sequence_length(unsigned char first)
{
    if (first <= 0x7F) {
        return 1;
    }

    if (first >= 0xC2 && first <= 0xDF) {
        return 2;
    }

    if (first >= 0xE0 && first <= 0xEF) {
        return 3;
    }

    if (first >= 0xF0 && first <= 0xF4) {
        return 4;
    }

    return 0;
}

static bool is_shortest_utf8(uint32_t value, size_t length)
{
    switch (length) {
        case 1:
            return value <= 0x7F;
        case 2:
            return value >= 0x80;
        case 3:
            return value >= 0x800;
        case 4:
            return value >= 0x10000;
        default:
            return false;
    }
}

bool decode_utf8(const char* chars, size_t length, DecodedScalar* scalar)
{
    if (length == 0) {
        return false;
    }

    unsigned char first = (unsigned char)chars[0];
    size_t count = utf8_sequence_length(first);

    if (count == 0 || count > length) {
        return false;
    }

    static const unsigned char masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t value = first & masks[count];

    for (size_t i = 1; i < count; i++) {
        unsigned char next = (unsigned char)chars[i];

        if ((next & 0xC0) != 0x80) {
            return false;
        }

        value = (value << 6) | (next & 0x3F);
    }

    bool shortest = is_shortest_utf8(value, count);
    bool valid = is_unicode_scalar(value);

    if (!shortest || !valid) {
        return false;
    }

    scalar->value = value;
    scalar->length = count;

    return true;
}

size_t encode_utf8(uint32_t value, char bytes[4])
{
    if (!is_unicode_scalar(value)) {
        return 0;
    }

    if (value <= 0x7F) {
        bytes[0] = (char)value;

        return 1;
    }

    if (value <= 0x7FF) {
        bytes[0] = (char)(0xC0 | (value >> 6));
        bytes[1] = (char)(0x80 | (value & 0x3F));

        return 2;
    }

    if (value <= 0xFFFF) {
        bytes[0] = (char)(0xE0 | (value >> 12));
        bytes[1] = (char)(0x80 | ((value >> 6) & 0x3F));
        bytes[2] = (char)(0x80 | (value & 0x3F));

        return 3;
    }

    bytes[0] = (char)(0xF0 | (value >> 18));
    bytes[1] = (char)(0x80 | ((value >> 12) & 0x3F));
    bytes[2] = (char)(0x80 | ((value >> 6) & 0x3F));
    bytes[3] = (char)(0x80 | (value & 0x3F));

    return 4;
}
