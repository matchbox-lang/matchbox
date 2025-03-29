#ifndef STRING_OBJECT_H
#define STRING_OBJECT_H

#include "object.h"

#define AS_STRING_OBJECT(value) ((StringObject*)AS_OBJECT(value))

typedef struct StringObject
{
    Object obj;
    char* chars;
    size_t length;
    size_t hash;
} StringObject;

StringObject* createStringObject(char* chars, size_t len, size_t hash);
StringObject* copyStringObject(const char* chars, size_t len);
void freeStringObject(StringObject* str);
bool compareStringObject(StringObject* a, StringObject* b);
size_t hashStringObject(const char* chars, size_t len);
void printStringObject(StringObject* str);

#endif
