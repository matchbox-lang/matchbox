#include "runtime/output.h"
#include "common/float_format.h"
#include "common/unicode.h"
#include "runtime/string_object.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool isPrintableType(TokenType type)
{
    return isIntegerTypeToken(type) || isFloatTypeToken(type)
        || type == TOKEN_BOOL
        || type == TOKEN_CHAR
        || type == TOKEN_STRING;
}

size_t writeValue(char* bytes, size_t capacity, Value* frame, TokenType type)
{
    switch (type) {
        case TOKEN_I8:
        case TOKEN_I16:
        case TOKEN_I32: {
            int32_t value = readSignedValue(frame[0], type);

            return snprintf(bytes, capacity, "%" PRId32, value);
        }
        case TOKEN_I64: {
            int64_t value = readI64(frame, 0);

            return snprintf(bytes, capacity, "%" PRId64, value);
        }
        case TOKEN_U8:
        case TOKEN_U16:
        case TOKEN_U32: {
            uint32_t value = readUnsignedValue(frame[0], type);

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

static void printValue(Value* values, TokenType type)
{
    if (type == TOKEN_STRING) {
        StringObject* string = AS_POINTER(values[0]);

        printStringObject(string);

        return;
    }

    char bytes[32];
    size_t length = writeValue(bytes, sizeof(bytes), values, type);

    fwrite(bytes, 1, length, stdout);
}

void printValues(Value* values, const TokenType* types, size_t count)
{
    size_t offset = 0;

    for (size_t i = 0; i < count; i++) {
        TokenType type = types[i];

        printValue(values + offset, type);

        offset += getTypeSlotCount(type);
    }
}
