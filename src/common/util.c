#include "common/util.h"
#include "common/memory.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

bool isLargerThan8BitSigned(int n)
{
    return n < SCHAR_MIN || n > SCHAR_MAX;
}

bool isLargerThan16BitSigned(int n)
{
    return n < SHRT_MIN || n > SHRT_MAX;
}

char* strndup(const char* source, size_t length)
{
    char* destination = allocateMemory(length + 1);
    memcpy(destination, source, length);
    destination[length] = '\0';

    return destination;
}
