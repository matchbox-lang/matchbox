#include "string_object.h"
#include "object.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StringObject* createStringObject(char* chars, size_t length, size_t hash)
{
    StringObject* string = ALLOCATE_OBJECT(StringObject, OBJ_STRING);
    string->chars = chars;
    string->length = length;
    string->hash = hash;

    return string;
}

StringObject* copyStringObject(const char* chars, size_t length)
{
    size_t hash = hashStringObject(chars, length);
    char* dst = strndup(chars, length);

    return createStringObject(dst, length, hash);
}

void freeStringObject(StringObject* string)
{
    free(string->chars);
    free(string);
}

bool compareStringObject(StringObject* a, StringObject* b)
{
    if (a->length == b->length) {
        return strncmp(a->chars, b->chars, b->length) == 0;
    }

    return false;
}

size_t hashStringObject(const char* chars, size_t length)
{
    size_t hash = 0;

    for (size_t i = 0; i < length; i++) {
        hash = chars[i] + 31 * hash;
    }

    return hash;
}

void printStringObject(StringObject* string)
{
    printf("%.*s", string->length, string->chars);
}
