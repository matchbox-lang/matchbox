#ifndef LITERAL_H
#define LITERAL_H

#include "common/unicode.h"
#include "compiler/token.h"

#define LITERAL_SCALAR_MAX_LENGTH 10

bool decode_literal_scalar(const char* chars, size_t length, DecodedScalar* scalar);
bool decode_string_contents(const char* chars, size_t length, char* bytes, size_t* count);

uint64_t parse_decimal_literal(Token token);
uint64_t parse_binary_literal(Token token);
uint64_t parse_hexadecimal_literal(Token token);
uint64_t parse_octal_literal(Token token);
double parse_float_literal(Token token);

#endif
