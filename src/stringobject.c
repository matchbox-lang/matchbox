#include "stringobject.h"
#include "object.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

StringObject* createStringObject(char* chars, size_t length, size_t hash)
{
    StringObject* string = ALLOCATE_OBJECT(StringObject, OBJ_STRING);
    string->chars = chars;
    string->length = length;
    string->hash = hash;

    return string;
}

StringObject* copyStringObject(const char* src, size_t len)
{
    size_t hash = hashStringObject(src, len);
    char* dst = strndup(src, len);

    return createStringObject(dst, len, hash);
}

void freeStringObject(StringObject* str)
{
    free(str->chars);
    free(str);
}

bool compareStringObject(StringObject* a, StringObject* b)
{
    if (a->length == b->length) {
        return strncmp(a->chars, b->chars, b->length) == 0;
    }

    return false;
}

size_t hashStringObject(const char* chars, size_t len)
{
    size_t hash = 0;

    for (size_t i = 0; i < len; i++) {
        hash = chars[i] + 31 * hash;
    }

    return hash;
}

void printStringObject(StringObject* str)
{
    printf("%.*s", str->length, str->chars);
}
