#ifndef STRING_OBJECT_H
#define STRING_OBJECT_H

#include "runtime/object.h"
#include "common/string_view.h"

#define AS_STRING_OBJECT(value) ((StringObject*)AS_OBJECT(value))

typedef struct StringObject
{
    Object obj;
    size_t length;
    size_t hash;
    char chars[];
} StringObject;

StringObject* createStringObject(const char* chars, size_t length, size_t hash);
StringObject* copyStringObject(const char* chars, size_t length);
StringView getStringView(const StringObject* string);
void freeStringObject(StringObject* string);
bool compareStringObject(const StringObject* a, const StringObject* b);
void printStringObject(StringObject* string);

#endif
