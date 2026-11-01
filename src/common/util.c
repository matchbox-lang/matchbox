#include "common/util.h"
#include "common/memory.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

char* strndup(const char* source, size_t length)
{
    char* destination = allocate_memory(length + 1);
    memcpy(destination, source, length);
    destination[length] = '\0';

    return destination;
}

bool is_larger_than8_bit_signed(int n)
{
    return n < SCHAR_MIN || n > SCHAR_MAX;
}

bool is_larger_than16_bit_signed(int n)
{
    return n < SHRT_MIN || n > SHRT_MAX;
}
