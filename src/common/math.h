#ifndef COMMON_MATH_H
#define COMMON_MATH_H

#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ABS(a) ((a) < 0 ? -(a) : (a))

int64_t ipow(int64_t base, int64_t exponent);

#endif
