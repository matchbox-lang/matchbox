#include "runtime/concatenation.h"
#include "common/errors.h"
#include "common/util.h"
#include "runtime/object.h"
#include <stdint.h>

StringObject* concatenate_string_parts(size_t capacity, StringPartsWriter write_parts, const void* context)
{
    Writer writer;
    size_t max_length = SIZE_MAX - sizeof(StringObject) - 1;

    if (capacity > max_length) {
        out_of_memory_error();
    }

    size_t size = sizeof(StringObject) + capacity + 1;
    StringObject* string = (StringObject*)allocate_object(size, ObjString);

    init_buffer_writer(&writer, string->chars, capacity);
    write_parts(&writer, context);

    string->length = writer.length;
    string->chars[writer.length] = '\0';
    string->hash = hash_string_view(string->chars, writer.length);

    return string;
}
