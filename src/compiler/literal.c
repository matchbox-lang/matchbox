#include "compiler/literal.h"
#include "common/memory.h"
#include "common/util.h"
#include "compiler/diagnostics.h"
#include <errno.h>
#include <stdlib.h>

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

static _Noreturn void floatOutsideSupportedRangeError(Token token)
{
    fatalSourceError(token.line, token.column, "Float is outside the supported range");
}

static _Noreturn void integerExceedsMaximumSizeError(Token token)
{
    fatalSourceError(token.line, token.column, "Integer exceeds the maximum supported size");
}

static void copyLiteralDigit(char c, char** destination, size_t* length)
{
    if (c == '_') {
        --(*length);

        return;
    }

    **destination = c;
    (*destination)++;
}

static void stripUnderscores(char* str, size_t* length)
{
    char* source = str;
    char* destination = str;

    while (*source != '\0') {
        copyLiteralDigit(*source, &destination, length);

        source++;
    }

    *destination = '\0';
}

static uint64_t parseIntegerLiteralInBase(Token token, size_t offset, int base)
{
    size_t length = token.length - offset;
    char* tmp = strndup(token.chars + offset, length);

    stripUnderscores(tmp, &length);

    errno = 0;
    uint64_t value = strtoull(tmp, NULL, base);

    if (errno == ERANGE) {
        integerExceedsMaximumSizeError(token);
    }

    freeMemory(tmp);

    return value;
}

uint64_t parseDecimalLiteral(Token token)
{
    return parseIntegerLiteralInBase(token, 0, 10);
}

uint64_t parseBinaryLiteral(Token token)
{
    return parseIntegerLiteralInBase(token, 2, 2);
}

uint64_t parseHexadecimalLiteral(Token token)
{
    return parseIntegerLiteralInBase(token, 2, 16);
}

uint64_t parseOctalLiteral(Token token)
{
    return parseIntegerLiteralInBase(token, 2, 8);
}

double parseFloatLiteral(Token token)
{
    size_t length = token.length;
    char* tmp = strndup(token.chars, length);

    stripUnderscores(tmp, &length);

    errno = 0;
    double value = strtod(tmp, NULL);

    if (errno == ERANGE) {
        floatOutsideSupportedRangeError(token);
    }

    freeMemory(tmp);

    return value;
}
