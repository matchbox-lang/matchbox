#ifndef STRING_OBJECT_H
#define STRING_OBJECT_H

#include "common/string_view.h"
#include "runtime/object.h"

#define AS_STRING_OBJECT(value) ((StringObject*)AS_OBJECT(value))

typedef struct StringObject
{
    Object obj;
    size_t length;
    size_t hash;
    char chars[];
} StringObject;

StringObject* create_string_object(const char* chars, size_t length, size_t hash);
StringObject* copy_string_object(const char* chars, size_t length);
StringView get_string_view(const StringObject* string);
void free_string_object(StringObject* string);
bool compare_string_object(const StringObject* a, const StringObject* b);
void print_string_object(const StringObject* string);

#endif
