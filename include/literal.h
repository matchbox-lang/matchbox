#ifndef LITERAL_H
#define LITERAL_H

#include "unicode.h"

#define LITERAL_SCALAR_MAX_LENGTH 10

bool decodeLiteralScalar(const char* chars, size_t length, DecodedScalar* scalar);

#endif
