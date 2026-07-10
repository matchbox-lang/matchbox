#ifndef CONVERSION_H
#define CONVERSION_H

#include <stddef.h>

float integerLiteralToValue(char* str, size_t length);
int binaryLiteralToValue(char* str, size_t length);
int hexadecimalLiteralToValue(char* str, size_t length);
int octalLiteralToValue(char* str, size_t length);
int floatLiteralToValue(char* str, size_t length);

#endif
