#include "string_concatenation.h"
#include "object.h"
#include "output.h"
#include "util.h"
#include <stdint.h>

static size_t stringPartCapacity(Value* values, TokenType type)
{
    switch (type) {
        case TOKEN_STRING: {
            StringObject* string = AS_POINTER(values[0]);

            return string->length;
        }
        case TOKEN_I8:
            return 4;
        case TOKEN_U8:
            return 3;
        case TOKEN_I16:
            return 6;
        case TOKEN_U16:
        case TOKEN_BOOL:
            return 5;
        case TOKEN_I32:
            return 11;
        case TOKEN_U32:
            return 10;
        case TOKEN_I64:
        case TOKEN_U64:
            return 20;
        case TOKEN_CHAR:
            return 4;
        case TOKEN_F32:
        case TOKEN_F64:
            return 32;
        default:
            return 0;
    }
}

static size_t stringPartsCapacity(Value* values, const TokenType* types, size_t count)
{
    size_t capacity = 0;
    size_t maxLength = SIZE_MAX - sizeof(StringObject) - 1;
    size_t offset = 0;

    for (size_t i = 0; i < count; i++) {
        TokenType type = types[i];
        size_t length = stringPartCapacity(values + offset, type);

        if (length > maxLength - capacity) {
            outOfMemoryError();
        }

        capacity += length;
        offset += getTypeSlotCount(type);
    }

    return capacity;
}

StringObject* concatenateStringParts(Value* values, const TokenType* types, size_t count)
{
    // Reserve bounded space so each value is written directly into the result once
    size_t capacity = stringPartsCapacity(values, types, count);
    size_t size = sizeof(StringObject) + capacity + 1;
    StringObject* string = (StringObject*)allocateObject(size, OBJ_STRING);
    size_t length = 0;
    size_t offset = 0;

    for (size_t i = 0; i < count; i++) {
        TokenType type = types[i];
        size_t written = writeValue(string->chars + length, capacity - length + 1, values + offset, type);
        length += written;
        offset += getTypeSlotCount(type);
    }

    string->length = length;
    string->chars[length] = '\0';
    string->hash = hashStringView(string->chars, length);

    return string;
}
