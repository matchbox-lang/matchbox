#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>

typedef struct StringView
{
    const char* chars;
    size_t length;
    size_t hash;
} StringView;

StringView createStringView(const char* chars, size_t length);
bool compareStringView(StringView a, StringView b);
size_t hashStringView(const char* chars, size_t length);

#endif
