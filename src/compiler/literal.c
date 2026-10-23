#include "compiler/literal.h"

static bool decodeHexDigit(char c, uint32_t* value)
{
    if (c >= '0' && c <= '9') {
        *value = c - '0';

        return true;
    }

    if (c >= 'a' && c <= 'f') {
        *value = c - 'a' + 10;

        return true;
    }

    if (c >= 'A' && c <= 'F') {
        *value = c - 'A' + 10;

        return true;
    }

    return false;
}

static bool decodeUnicodeEscape(const char* chars, size_t length, DecodedScalar* scalar)
{
    if (length < 5 || chars[2] != '{') {
        return false;
    }

    uint32_t value = 0;
    size_t position = 3;

    while (position < length && chars[position] != '}') {
        uint32_t digit = 0;
        bool validDigit = decodeHexDigit(chars[position], &digit);

        if (position - 3 == 6 || !validDigit) {
            return false;
        }

        value = (value << 4) | digit;
        position++;
    }

    if (position == 3 || position == length) {
        return false;
    }

    if (!isUnicodeScalar(value)) {
        return false;
    }

    scalar->value = value;
    scalar->length = position + 1;

    return true;
}

static bool decodeEscape(const char* chars, size_t length, DecodedScalar* scalar)
{
    if (length < 2) {
        return false;
    }

    uint32_t value;

    switch (chars[1]) {
        case 'n':
            value = '\n';
            break;
        case 'r':
            value = '\r';
            break;
        case 't':
            value = '\t';
            break;
        case '0':
            value = 0;
            break;
        case '\\':
        case '\'':
        case '"':
            value = (unsigned char)chars[1];
            break;
        case 'u':
            return decodeUnicodeEscape(chars, length, scalar);
        default:
            return false;
    }

    scalar->value = value;
    scalar->length = 2;

    return true;
}

bool decodeLiteralScalar(const char* chars, size_t length, DecodedScalar* scalar)
{
    if (length == 0) {
        return false;
    }

    if (chars[0] == '\\') {
        return decodeEscape(chars, length, scalar);
    }

    return decodeUtf8(chars, length, scalar);
}

bool decodeStringContents(const char* chars, size_t length, char* bytes, size_t* count)
{
    *count = 0;

    while (length > 0) {
        DecodedScalar scalar;
        bool valid = decodeLiteralScalar(chars, length, &scalar);

        if (!valid) {
            return false;
        }

        size_t encodedLength = encodeUtf8(scalar.value, bytes + *count);
        *count += encodedLength;
        chars += scalar.length;
        length -= scalar.length;
    }

    return true;
}
