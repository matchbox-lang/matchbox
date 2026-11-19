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

static bool has_same_f64_bits(double left, double right)
{
    uint64_t left_bits;
    uint64_t right_bits;
    
    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));

    return left_bits == right_bits;
}

static bool round_trips_float(const char* text, double value, int digits)
{
    double parsed;

    if (digits == 9) {
        parsed = strtof(text, NULL);
    } else {
        parsed = strtod(text, NULL);
    }

    return has_same_f64_bits(value, parsed);
}

static size_t select_shortest_float(char* buffer, size_t size, char* candidate, bool round_trip, size_t shortest)
{
    if (!round_trip) {
        return shortest;
    }

    normalize_float_exponent(candidate, 32);

    size_t length = strlen(candidate);

    if (length >= shortest) {
        return shortest;
    }

    snprintf(buffer, size, "%s", candidate);

    return length;
}

static void format_shortest_float(char* buffer, size_t size, double value, int digits)
{
    if (format_special_float(buffer, size, value)) {
        return;
    }

    char candidate[32];
    size_t shortest = SIZE_MAX;

    snprintf(buffer, size, "%.*g", digits, value);
    normalize_float_exponent(buffer, size);

    for (int precision = 1; precision <= digits; precision++) {
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);

        bool round_trip = round_trips_float(candidate, value, digits);
        shortest = select_shortest_float(buffer, size, candidate, round_trip, shortest);
    }

    append_float_fraction(buffer, size);
}

void format_shortest_f32(char* buffer, size_t size, float value)
{
    format_shortest_float(buffer, size, value, 9);
}

void format_shortest_f64(char* buffer, size_t size, double value)
{
    format_shortest_float(buffer, size, value, 17);
}
