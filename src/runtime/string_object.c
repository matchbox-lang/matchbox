#include "runtime/string_object.h"
#include "runtime/object.h"
#include "common/util.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StringObject* createStringObject(const char* chars, size_t length, size_t hash)
{
    if (length > SIZE_MAX - sizeof(StringObject) - 1) {
        outOfMemoryError();
    }

    size_t size = sizeof(StringObject) + length + 1;
    StringObject* string = (StringObject*)allocateObject(size, OBJ_STRING);

    string->length = length;
    string->hash = hash;

    if (length > 0) {
        memcpy(string->chars, chars, length);
    }

    string->chars[length] = '\0';

    return string;
}

StringObject* copyStringObject(const char* chars, size_t length)
{
    size_t hash = hashStringView(chars, length);

    return createStringObject(chars, length, hash);
}

StringView getStringView(const StringObject* string)
{
    StringView view;
    view.chars = string->chars;
    view.length = string->length;
    view.hash = string->hash;

    return view;
}

void freeStringObject(StringObject* string)
{
    free(string);
}

bool compareStringObject(const StringObject* a, const StringObject* b)
{
    StringView left = getStringView(a);
    StringView right = getStringView(b);

    return compareStringView(left, right);
}

void printStringObject(StringObject* string)
{
    fwrite(string->chars, 1, string->length, stdout);
}
