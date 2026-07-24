#include "conversion.h"
#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t literalToValue(char* str, size_t length, int base)
{
    char* tmp = strndup(str, length);
    stripUnderscores(tmp, &length);
    errno = 0;
    uint64_t value = strtoull(tmp, NULL, base);

    if (errno == ERANGE) {
        fprintf(stderr, "Error: Integer literal exceeds the u64 range\n");
        free(tmp);
        exit(1);
    }

    free(tmp);

    return value;
}

uint64_t integerLiteralToValue(char* str, size_t length)
{
    return literalToValue(str, length, 10);
}

uint64_t binaryLiteralToValue(char* str, size_t length)
{
    return literalToValue(str + 2, length - 2, 2);
}

uint64_t hexadecimalLiteralToValue(char* str, size_t length)
{
    return literalToValue(str + 2, length - 2, 16);
}

uint64_t octalLiteralToValue(char* str, size_t length)
{
    return literalToValue(str + 2, length - 2, 8);
}

float floatLiteralToValue(char* str, size_t length)
{
    char* tmp = strndup(str, length);
    stripUnderscores(tmp, &length);
    float value = strtod(tmp, NULL);
    free(tmp);

    return value;
}
