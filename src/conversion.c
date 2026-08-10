#include "conversion.h"
#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void integerExceedsMaximumSizeError(Token token)
{
    fprintf(stderr, "Error: Integer exceeds the maximum supported size");
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void floatOutsideSupportedRangeError(Token token)
{
    fprintf(stderr, "Error: Float is outside the supported range");
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static uint64_t integerLiteralToValueInBase(Token token, size_t offset, int base)
{
    size_t length = token.length - offset;
    char* tmp = strndup(token.chars + offset, length);
    stripUnderscores(tmp, &length);
    errno = 0;
    uint64_t value = strtoull(tmp, NULL, base);

    if (errno == ERANGE) {
        integerExceedsMaximumSizeError(token);
    }

    free(tmp);

    return value;
}

uint64_t integerLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 0, 10);
}

uint64_t binaryLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 2, 2);
}

uint64_t hexadecimalLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 2, 16);
}

uint64_t octalLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 2, 8);
}

double floatLiteralToValue(Token token)
{
    size_t length = token.length;
    char* tmp = strndup(token.chars, length);
    stripUnderscores(tmp, &length);
    errno = 0;
    double value = strtod(tmp, NULL);

    if (errno == ERANGE) {
        floatOutsideSupportedRangeError(token);
    }

    free(tmp);

    return value;
}
