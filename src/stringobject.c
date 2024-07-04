#include "stringobject.h"
#include "object.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

StringObject* createString(char* chars, int length, size_t hash)
{
    StringObject* string = malloc(sizeof(StringObject));
    string->obj.type = OBJ_STRING;
    string->chars = chars;
    string->length = length;
    string->hash = hash;

    return string;
}

StringObject* copyString(const char* src, int length)
{
    size_t hash = hashString(src, length);
    char* dst = malloc(sizeof(char) * length + 1);
    memcpy(dst, src, length);
    dst[length] = '\0';

    return createString(dst, length, hash);
}

void freeString(StringObject* string)
{
    free(string->chars);
    free(string);
}

bool compareString(StringObject* a, StringObject* b)
{
    if (a->length == b->length) {
        return strncmp(a->chars, b->chars, b->length) == 0;
    }

    return false;
}

size_t hashString(const char* chars, int length)
{
    size_t hash = 0;

    for (size_t i = 0; i < length; i++) {
        hash = chars[i] + 31 * hash;
    }

    return hash;
}

void printString(StringObject* string)
{
    printf("%.*s\n", string->length, string->chars);
}
