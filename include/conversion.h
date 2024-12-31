#ifndef CONVERSION_H
#define CONVERSION_H

#include <stddef.h>

float decimalLiteralToValue(char* str, size_t len);
int binaryLiteralToValue(char* str, size_t len);
int hexadecimalLiteralToValue(char* str, size_t len);
int octalLiteralToValue(char* str, size_t len);
int floatLiteralToValue(char* str, size_t len);

#endif
