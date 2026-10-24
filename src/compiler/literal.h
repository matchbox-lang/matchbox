#ifndef LITERAL_H
#define LITERAL_H

#include "common/unicode.h"

#define LITERAL_SCALAR_MAX_LENGTH 10

bool decodeLiteralScalar(const char* chars, size_t length, DecodedScalar* scalar);
bool decodeStringContents(const char* chars, size_t length, char* bytes, size_t* count);

#endif
