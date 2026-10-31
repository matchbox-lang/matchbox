#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ABS(a) ((a) < 0 ? -(a) : (a))

bool isLargerThan8BitSigned(int n);
bool isLargerThan16BitSigned(int n);
char* strndup(const char* source, size_t length);

#endif
