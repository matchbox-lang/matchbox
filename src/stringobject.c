#include "stringobject.h"
#include "object.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <util.h>

StringObject* createString(char* chars, size_t length, size_t hash)
{
    StringObject* str = malloc(sizeof(StringObject));
    str->obj.type = OBJ_STRING;
    str->chars = chars;
    str->length = length;
    str->hash = hash;

    return str;
}

StringObject* copyString(const char* src, size_t len)
{
    size_t hash = hashString(src, len);
    char* dst = strndup(src, len);

    return createString(dst, len, hash);
}

void freeString(StringObject* str)
{
    free(str->chars);
    free(str);
}

bool compareString(StringObject* a, StringObject* b)
{
    if (a->length == b->length) {
        return strncmp(a->chars, b->chars, b->length) == 0;
    }

    return false;
}

size_t hashString(const char* chars, size_t len)
{
    size_t hash = 0;

    for (size_t i = 0; i < len; i++) {
        hash = chars[i] + 31 * hash;
    }

    return hash;
}

void printString(StringObject* str)
{
    printf("%.*s", str->length, str->chars);
}
