#include "common/float_format.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool format_special_float(char* buffer, size_t size, double value)
{
    if (isnan(value)) {
        snprintf(buffer, size, "NaN");

        return true;
    }

    if (value == INFINITY) {
        snprintf(buffer, size, "Infinity");

        return true;
    }

    if (value == -INFINITY) {
        snprintf(buffer, size, "-Infinity");

        return true;
    }

    return false;
}

static void normalize_float_exponent(char* buffer, size_t size)
{
    if (size == 0) {
        return;
    }

    char* exponent = strchr(buffer, 'e');
    if (!exponent) {
        return;
    }

    exponent++;
    if (*exponent == '+' || *exponent == '-') {
        exponent++;
    }

    char* digits = exponent;
    while (digits[0] == '0' && digits[1] != '\0') {
        digits++;
    }

    size_t length = strlen(digits) + 1;

    memmove(exponent, digits, length);
}

static void append_float_fraction(char* buffer, size_t size)
{
    if (size == 0) {
        return;
    }

    size_t length = strlen(buffer);
    size_t integer_length = strspn(buffer, "-0123456789");

    if (length == 0 || integer_length != length) {
        return;
    }

    snprintf(buffer + length, size - length, ".0");
}

static bool same_f32_bits(float left, float right)
{
    uint32_t left_bits;
    uint32_t right_bits;

    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));

    return left_bits == right_bits;
}

static bool same_f64_bits(double left, double right)
{
    uint64_t left_bits;
    uint64_t right_bits;
    
    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));

    return left_bits == right_bits;
}

void format_shortest_f32(char* buffer, size_t size, float value)
{
    if (format_special_float(buffer, size, value)) {
        return;
    }

    char candidate[32];
    size_t shortest = SIZE_MAX;

    snprintf(buffer, size, "%.9g", value);
    normalize_float_exponent(buffer, size);

    for (int precision = 1; precision <= 9; precision++) {
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        float parsed = strtof(candidate, NULL);

        if (!same_f32_bits(value, parsed)) {
            continue;
        }

        normalize_float_exponent(candidate, sizeof(candidate));

        size_t length = strlen(candidate);
        if (length >= shortest) {
            continue;
        }

        shortest = length;
        snprintf(buffer, size, "%s", candidate);
    }

    append_float_fraction(buffer, size);
}

void format_shortest_f64(char* buffer, size_t size, double value)
{
    if (format_special_float(buffer, size, value)) {
        return;
    }

    char candidate[32];
    size_t shortest = SIZE_MAX;

    snprintf(buffer, size, "%.17g", value);
    normalize_float_exponent(buffer, size);

    for (int precision = 1; precision <= 17; precision++) {
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        double parsed = strtod(candidate, NULL);

        if (!same_f64_bits(value, parsed)) {
            continue;
        }

        normalize_float_exponent(candidate, sizeof(candidate));

        size_t length = strlen(candidate);
        if (length >= shortest) {
            continue;
        }

        shortest = length;
        snprintf(buffer, size, "%s", candidate);
    }

    append_float_fraction(buffer, size);
}
