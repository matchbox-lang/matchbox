#include "common/math.h"
#include "common/errors.h"

static int64_t negative_ipow(int64_t base, int64_t exponent)
{
    if (base == 0) {
        fatal_error("Zero cannot be raised to a negative power");
    }

    if (base == 1) {
        return 1;
    }

    if (base == -1) {
        return (exponent % 2 == 0) ? 1 : -1;
    }

    return 0;
}

int64_t ipow(int64_t base, int64_t exponent)
{
    if (exponent < 0) {
        return negative_ipow(base, exponent);
    }

    int64_t result = 1;

    while (exponent > 0) {
        if (exponent & 1) {
            result *= base;
        }

        exponent >>= 1;

        if (exponent > 0) {
            base *= base;
        }
    }

    return result;
}
