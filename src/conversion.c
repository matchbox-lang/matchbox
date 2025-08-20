#include "conversion.h"
#include "util.h"
#include <stdlib.h>

float integerLiteralToValue(char* str, size_t len)
{
    char* tmp = strndup(str, len);
    stripUnderscores(tmp, &len);
    int value = strtol(tmp, NULL, 10);
    free(tmp);

    return value;
}

int binaryLiteralToValue(char* str, size_t len)
{
    char* tmp = strndup(str + 2, len - 2);
    stripUnderscores(tmp, &len);
    int value = strtol(tmp, NULL, 2);
    free(tmp);

    return value;
}

int hexadecimalLiteralToValue(char* str, size_t len)
{
    char* tmp = strndup(str + 2, len - 2);
    stripUnderscores(tmp, &len);
    int value = strtol(tmp, NULL, 16);
    free(tmp);

    return value;
}

int octalLiteralToValue(char* str, size_t len)
{
    char* tmp = strndup(str + 2, len - 2);
    stripUnderscores(tmp, &len);
    int value = strtol(tmp, NULL, 8);
    free(tmp);

    return value;
}

int floatLiteralToValue(char* str, size_t len)
{
    char* tmp = strndup(str, len);
    stripUnderscores(tmp, &len);
    float value = strtod(tmp, NULL);
    free(tmp);

    return value;
}
