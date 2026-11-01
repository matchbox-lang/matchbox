#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ABS(a) ((a) < 0 ? -(a) : (a))

char* strndup(const char* source, size_t length);
bool is_larger_than8_bit_signed(int n);
bool is_larger_than16_bit_signed(int n);

#endif
