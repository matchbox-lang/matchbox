#ifndef LITERAL_H
#define LITERAL_H

#include "common/unicode.h"
#include "compiler/token.h"

#define LITERAL_SCALAR_MAX_LENGTH 10

bool decodeLiteralScalar(const char* chars, size_t length, DecodedScalar* scalar);
bool decodeStringContents(const char* chars, size_t length, char* bytes, size_t* count);

uint64_t parseDecimalLiteral(Token token);
uint64_t parseBinaryLiteral(Token token);
uint64_t parseHexadecimalLiteral(Token token);
uint64_t parseOctalLiteral(Token token);
double parseFloatLiteral(Token token);

#endif
