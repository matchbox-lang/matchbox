#include "conversion.h"
#include "util.h"
#include <stdlib.h>

float integerLiteralToValue(char* str, size_t length)
{
    char* tmp = strndup(str, length);
    stripUnderscores(tmp, &length);
    int value = strtol(tmp, NULL, 10);
    free(tmp);

    return value;
}

int binaryLiteralToValue(char* str, size_t length)
{
    char* tmp = strndup(str + 2, length - 2);
    stripUnderscores(tmp, &length);
    int value = strtol(tmp, NULL, 2);
    free(tmp);

    return value;
}

int hexadecimalLiteralToValue(char* str, size_t length)
{
    char* tmp = strndup(str + 2, length - 2);
    stripUnderscores(tmp, &length);
    int value = strtol(tmp, NULL, 16);
    free(tmp);

    return value;
}

int octalLiteralToValue(char* str, size_t length)
{
    char* tmp = strndup(str + 2, length - 2);
    stripUnderscores(tmp, &length);
    int value = strtol(tmp, NULL, 8);
    free(tmp);

    return value;
}

int floatLiteralToValue(char* str, size_t length)
{
    char* tmp = strndup(str, length);
    stripUnderscores(tmp, &length);
    float value = strtod(tmp, NULL);
    free(tmp);

    return value;
}
