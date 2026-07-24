#ifndef CONVERSION_H
#define CONVERSION_H

#include <stddef.h>
#include <stdint.h>

uint64_t integerLiteralToValue(char* str, size_t length);
uint64_t binaryLiteralToValue(char* str, size_t length);
uint64_t hexadecimalLiteralToValue(char* str, size_t length);
uint64_t octalLiteralToValue(char* str, size_t length);
float floatLiteralToValue(char* str, size_t length);

#endif
