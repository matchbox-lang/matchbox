#include "common/string_view.h"
#include <string.h>

StringView createStringView(const char* chars, size_t length)
{
    StringView view;
    view.chars = chars;
    view.length = length;
    view.hash = hashStringView(chars, length);

    return view;
}

bool compareStringView(StringView a, StringView b)
{
    if (a.length != b.length) {
        return false;
    }

    if (a.length == 0) {
        return true;
    }

    return memcmp(a.chars, b.chars, a.length) == 0;
}

size_t hashStringView(const char* chars, size_t length)
{
    size_t hash = 0;

    for (size_t i = 0; i < length; i++) {
        hash = (unsigned char)chars[i] + 31 * hash;
    }

    return hash;
}
