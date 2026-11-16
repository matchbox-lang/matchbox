#include "common/util.h"
#include "common/memory.h"
#include <string.h>

char* strndup(const char* source, size_t length)
{
    char* destination = allocate_memory(length + 1);
    memcpy(destination, source, length);
    destination[length] = '\0';

    return destination;
}
