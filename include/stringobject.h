#ifndef STRINGOBJECT_H
#define STRINGOBJECT_H

#include "object.h"
#include "value.h"

#define AS_STRINGOBJECT(value) ((StringObject*)AS_POINTER(value))

typedef struct StringObject
{
    Object obj;
    char* chars;
    size_t length;
    size_t hash;
} StringObject;

StringObject* createString(char* chars, int length, size_t hash);
StringObject* copyString(const char* chars, int length);
void freeString(StringObject* string);
bool compareString(StringObject* a, StringObject* b);
size_t hashString(const char* chars, int length);
void printString(StringObject* string);

#endif
