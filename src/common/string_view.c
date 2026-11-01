#include "common/string_view.h"
#include <string.h>

StringView create_string_view(const char* chars, size_t length)
{
    StringView view;
    view.chars = chars;
    view.length = length;
    view.hash = hash_string_view(chars, length);

    return view;
}

bool compare_string_view(StringView a, StringView b)
{
    if (a.length != b.length) {
        return false;
    }

    if (a.length == 0) {
        return true;
    }

    return memcmp(a.chars, b.chars, a.length) == 0;
}

size_t hash_string_view(const char* chars, size_t length)
{
    size_t hash = 0;

    for (size_t i = 0; i < length; i++) {
        hash = (unsigned char)chars[i] + 31 * hash;
    }

    return hash;
}
