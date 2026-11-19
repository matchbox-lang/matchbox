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

StringView create_string_view(const char* chars, size_t length);
bool string_views_equal(StringView a, StringView b);
size_t hash_string_view(const char* chars, size_t length);

#endif
