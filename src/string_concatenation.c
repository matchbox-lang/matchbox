#include "string_concatenation.h"
#include "object.h"
#include "unicode.h"
#include "util.h"
#include <stdint.h>
#include <string.h>

static StringView concatenationView(Value value, TokenType type, char bytes[4])
{
    if (type == TOKEN_CHAR) {
        uint32_t scalar = AS_U32(value);
        size_t length = encodeUtf8(scalar, bytes);

        return createStringView(bytes, length);
    }

    StringObject* string = AS_POINTER(value);

    return getStringView(string);
}

StringObject* concatenateStringValues(Value left, Value right, TokenType leftType, TokenType rightType)
{
    char leftBytes[4];
    char rightBytes[4];
    StringView leftView = concatenationView(left, leftType, leftBytes);
    StringView rightView = concatenationView(right, rightType, rightBytes);

    size_t maxLength = SIZE_MAX - sizeof(StringObject) - 1;

    if (rightView.length > maxLength || leftView.length > maxLength - rightView.length) {
        outOfMemoryError();
    }

    size_t length = leftView.length + rightView.length;
    size_t size = sizeof(StringObject) + length + 1;
    StringObject* string = (StringObject*)allocateObject(size, OBJ_STRING);

    memcpy(string->chars, leftView.chars, leftView.length);
    memcpy(string->chars + leftView.length, rightView.chars, rightView.length);

    string->length = length;
    string->hash = hashStringView(string->chars, length);
    string->chars[length] = '\0';

    return string;
}
