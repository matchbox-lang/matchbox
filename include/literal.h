#ifndef LITERAL_H
#define LITERAL_H

#include "unicode.h"

#define LITERAL_SCALAR_MAX_LENGTH 10

bool decodeLiteralScalar(const char* chars, size_t length, DecodedScalar* scalar);
// The output buffer must hold at least length bytes; count receives the decoded byte length
bool decodeStringContents(const char* chars, size_t length, char* bytes, size_t* count);

#endif
