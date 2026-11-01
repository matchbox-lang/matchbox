#ifndef CONCATENATION_H
#define CONCATENATION_H

#include "runtime/string_object.h"
#include "runtime/writer.h"

typedef void (*StringPartsWriter)(Writer* writer, const void* context);

StringObject* concatenate_string_parts(size_t capacity, StringPartsWriter write_parts, const void* context);

#endif
