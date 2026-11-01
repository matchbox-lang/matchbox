#include "compiler/literal.h"
#include "common/memory.h"
#include "common/util.h"
#include "compiler/diagnostics.h"
#include <errno.h>
#include <stdlib.h>

static bool decode_hex_digit(char c, uint32_t* value)
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

static bool decode_unicode_escape(const char* chars, size_t length, DecodedScalar* scalar)
{
    if (length < 5 || chars[2] != '{') {
        return false;
    }

    uint32_t value = 0;
    size_t position = 3;

    while (position < length && chars[position] != '}') {
        uint32_t digit = 0;
        bool valid_digit = decode_hex_digit(chars[position], &digit);

        if (position - 3 == 6 || !valid_digit) {
            return false;
        }

        value = (value << 4) | digit;
        position++;
    }

    if (position == 3 || position == length) {
        return false;
    }

    if (!is_unicode_scalar(value)) {
        return false;
    }

    scalar->value = value;
    scalar->length = position + 1;

    return true;
}

static bool decode_escape(const char* chars, size_t length, DecodedScalar* scalar)
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
            return decode_unicode_escape(chars, length, scalar);
        default:
            return false;
    }

    scalar->value = value;
    scalar->length = 2;

    return true;
}

bool decode_literal_scalar(const char* chars, size_t length, DecodedScalar* scalar)
{
    if (length == 0) {
        return false;
    }

    if (chars[0] == '\\') {
        return decode_escape(chars, length, scalar);
    }

    return decode_utf8(chars, length, scalar);
}

bool decode_string_contents(const char* chars, size_t length, char* bytes, size_t* count)
{
    *count = 0;

    while (length > 0) {
        DecodedScalar scalar;
        bool valid = decode_literal_scalar(chars, length, &scalar);

        if (!valid) {
            return false;
        }

        size_t encoded_length = encode_utf8(scalar.value, bytes + *count);
        *count += encoded_length;
        chars += scalar.length;
        length -= scalar.length;
    }

    return true;
}

static _Noreturn void float_outside_supported_range_error(Token token)
{
    fatal_source_error(token.line, token.column, "Float is outside the supported range");
}

static _Noreturn void integer_exceeds_maximum_size_error(Token token)
{
    fatal_source_error(token.line, token.column, "Integer exceeds the maximum supported size");
}

static void copy_literal_digit(char c, char** destination, size_t* length)
{
    if (c == '_') {
        --(*length);

        return;
    }

    **destination = c;
    (*destination)++;
}

static void strip_underscores(char* str, size_t* length)
{
    char* source = str;
    char* destination = str;

    while (*source != '\0') {
        copy_literal_digit(*source, &destination, length);

        source++;
    }

    *destination = '\0';
}

static uint64_t parse_integer_literal_in_base(Token token, size_t offset, int base)
{
    size_t length = token.length - offset;
    char* tmp = strndup(token.chars + offset, length);

    strip_underscores(tmp, &length);

    errno = 0;
    uint64_t value = strtoull(tmp, NULL, base);

    if (errno == ERANGE) {
        integer_exceeds_maximum_size_error(token);
    }

    free_memory(tmp);

    return value;
}

uint64_t parse_decimal_literal(Token token)
{
    return parse_integer_literal_in_base(token, 0, 10);
}

uint64_t parse_binary_literal(Token token)
{
    return parse_integer_literal_in_base(token, 2, 2);
}

uint64_t parse_hexadecimal_literal(Token token)
{
    return parse_integer_literal_in_base(token, 2, 16);
}

uint64_t parse_octal_literal(Token token)
{
    return parse_integer_literal_in_base(token, 2, 8);
}

double parse_float_literal(Token token)
{
    size_t length = token.length;
    char* tmp = strndup(token.chars, length);

    strip_underscores(tmp, &length);

    errno = 0;
    double value = strtod(tmp, NULL);

    if (errno == ERANGE) {
        float_outside_supported_range_error(token);
    }

    free_memory(tmp);

    return value;
}
