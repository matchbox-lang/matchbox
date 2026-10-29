#include "runtime/concatenation.h"
#include "common/errors.h"
#include "common/util.h"
#include "runtime/object.h"
#include <stdint.h>

StringObject* concatenateStringParts(size_t capacity, StringPartsWriter writeParts, const void* context)
{
    Writer writer;
    size_t maxLength = SIZE_MAX - sizeof(StringObject) - 1;

    if (capacity > maxLength) {
        outOfMemoryError();
    }

    size_t size = sizeof(StringObject) + capacity + 1;
    StringObject* string = (StringObject*)allocateObject(size, OBJ_STRING);

    initBufferWriter(&writer, string->chars, capacity);
    writeParts(&writer, context);

    string->length = writer.length;
    string->chars[writer.length] = '\0';
    string->hash = hashStringView(string->chars, writer.length);

    return string;
}
