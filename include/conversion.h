#ifndef CONVERSION_H
#define CONVERSION_H

#include "token.h"
#include <stddef.h>
#include <stdint.h>

uint64_t integerLiteralToValue(Token token);
uint64_t binaryLiteralToValue(Token token);
uint64_t hexadecimalLiteralToValue(Token token);
uint64_t octalLiteralToValue(Token token);
double floatLiteralToValue(char* str, size_t length);

#endif
