#ifndef CONCATENATION_H
#define CONCATENATION_H

#include "runtime/writer.h"
#include "runtime/string_object.h"

typedef void (*StringPartsWriter)(Writer* writer, const void* context);

StringObject* concatenateStringParts(size_t capacity, StringPartsWriter writeParts, const void* context);

#endif
