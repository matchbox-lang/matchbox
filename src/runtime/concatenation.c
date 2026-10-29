#include "runtime/concatenation.h"
#include "common/errors.h"
#include "common/util.h"
#include "runtime/object.h"
#include <stdint.h>

StringObject* concatenateStringParts(StringPartsWriter writeParts, const void* context)
{
    Writer writer;

    initBufferWriter(&writer, NULL, 0);
    writeParts(&writer, context);

    size_t length = writer.length;
    size_t maxLength = SIZE_MAX - sizeof(StringObject) - 1;

    if (length > maxLength) {
        outOfMemoryError();
    }

    size_t size = sizeof(StringObject) + length + 1;
    StringObject* string = (StringObject*)allocateObject(size, OBJ_STRING);

    initBufferWriter(&writer, string->chars, length);
    writeParts(&writer, context);

    string->length = writer.length;
    string->chars[writer.length] = '\0';
    string->hash = hashStringView(string->chars, writer.length);

    return string;
}
