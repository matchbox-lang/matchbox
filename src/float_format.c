#include "float_format.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool sameF32Bits(float left, float right)
{
    uint32_t leftBits;
    uint32_t rightBits;

    memcpy(&leftBits, &left, sizeof(leftBits));
    memcpy(&rightBits, &right, sizeof(rightBits));

    return leftBits == rightBits;
}

static bool sameF64Bits(double left, double right)
{
    uint64_t leftBits;
    uint64_t rightBits;
    
    memcpy(&leftBits, &left, sizeof(leftBits));
    memcpy(&rightBits, &right, sizeof(rightBits));

    return leftBits == rightBits;
}

void formatShortestF32(char* buffer, size_t size, float value)
{
    char candidate[32];
    size_t shortest = SIZE_MAX;
    snprintf(buffer, size, "%.9g", value);

    for (int precision = 1; precision <= 9; precision++) {
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        float parsed = strtof(candidate, NULL);

        if (!sameF32Bits(value, parsed)) {
            continue;
        }

        size_t length = strlen(candidate);
        if (length >= shortest) {
            continue;
        }

        shortest = length;
        snprintf(buffer, size, "%s", candidate);
    }
}

void formatShortestF64(char* buffer, size_t size, double value)
{
    char candidate[32];
    size_t shortest = SIZE_MAX;
    snprintf(buffer, size, "%.17g", value);

    for (int precision = 1; precision <= 17; precision++) {
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        double parsed = strtod(candidate, NULL);

        if (!sameF64Bits(value, parsed)) {
            continue;
        }

        size_t length = strlen(candidate);
        if (length >= shortest) {
            continue;
        }

        shortest = length;
        snprintf(buffer, size, "%s", candidate);
    }
}
