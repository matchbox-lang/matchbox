#include "string_concatenation.h"
#include "float_format.h"
#include "object.h"
#include "unicode.h"
#include "util.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool isStringPartType(TokenType type)
{
    bool integer = isIntegerTypeToken(type);
    bool floating = isFloatTypeToken(type);

    return integer || floating || type == TOKEN_BOOL || type == TOKEN_CHAR || type == TOKEN_STRING;
}

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

static size_t writeStringPart(char* bytes, size_t capacity, Value* frame, TokenType type)
{
    switch (type) {
        case TOKEN_I8:
        case TOKEN_I16:
        case TOKEN_I32: {
            int32_t value = AS_I32(frame[0]);

            return snprintf(bytes, capacity, "%" PRId32, value);
        }
        case TOKEN_I64: {
            int64_t value = readI64(frame, 0);

            return snprintf(bytes, capacity, "%" PRId64, value);
        }
        case TOKEN_U8:
        case TOKEN_U16:
        case TOKEN_U32: {
            uint32_t value = AS_U32(frame[0]);

            return snprintf(bytes, capacity, "%" PRIu32, value);
        }
        case TOKEN_U64: {
            uint64_t value = readU64(frame, 0);

            return snprintf(bytes, capacity, "%" PRIu64, value);
        }
        case TOKEN_F32: {
            float value = AS_F32(frame[0]);

            formatShortestF32(bytes, capacity, value);

            return strlen(bytes);
        }
        case TOKEN_F64: {
            double value = readF64(frame, 0);

            formatShortestF64(bytes, capacity, value);

            return strlen(bytes);
        }
        case TOKEN_BOOL: {
            const char* value = "false";

            if (AS_BOOL(frame[0])) {
                value = "true";
            }

            return snprintf(bytes, capacity, "%s", value);
        }
        case TOKEN_CHAR: {
            uint32_t value = AS_U32(frame[0]);

            return encodeUtf8(value, bytes);
        }
        case TOKEN_STRING: {
            StringObject* string = AS_POINTER(frame[0]);

            memcpy(bytes, string->chars, string->length);

            return string->length;
        }
        default:
            return 0;
    }
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
        size_t written = writeStringPart(string->chars + length, capacity - length + 1, values + offset, type);
        length += written;
        offset += getTypeSlotCount(type);
    }

    string->length = length;
    string->chars[length] = '\0';
    string->hash = hashStringView(string->chars, length);

    return string;
}
