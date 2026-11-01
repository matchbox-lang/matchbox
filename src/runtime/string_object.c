#include "runtime/string_object.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include "runtime/object.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StringObject* create_string_object(const char* chars, size_t length, size_t hash)
{
    if (length > SIZE_MAX - sizeof(StringObject) - 1) {
        out_of_memory_error();
    }

    size_t size = sizeof(StringObject) + length + 1;
    StringObject* string = (StringObject*)allocate_object(size, ObjString);

    string->length = length;
    string->hash = hash;

    if (length > 0) {
        memcpy(string->chars, chars, length);
    }

    string->chars[length] = '\0';

    return string;
}

StringObject* copy_string_object(const char* chars, size_t length)
{
    size_t hash = hash_string_view(chars, length);

    return create_string_object(chars, length, hash);
}

StringView get_string_view(const StringObject* string)
{
    StringView view;
    view.chars = string->chars;
    view.length = string->length;
    view.hash = string->hash;

    return view;
}

void free_string_object(StringObject* string)
{
    free_memory(string);
}

bool compare_string_object(const StringObject* a, const StringObject* b)
{
    StringView left = get_string_view(a);
    StringView right = get_string_view(b);

    return compare_string_view(left, right);
}

void print_string_object(const StringObject* string)
{
    fwrite(string->chars, 1, string->length, stdout);
}
