#include "util.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool isLargerThan8BitSigned(int n)
{
    return n < SCHAR_MIN || n > SCHAR_MAX;
}

bool isLargerThan16BitSigned(int n)
{
    return n < SHRT_MIN || n > SHRT_MAX;
}

void outOfMemoryError()
{
    fprintf(stderr, "Error: Out of memory\n");
    exit(1);
}

void stripUnderscores(char* str, size_t* length)
{
	char* src = str;
    char* dst = str;

	while (*src != '\0') {
		if (*src != '_') {
			*dst =* src;
			dst++;
		} else {
			--(*length);
		}

		src++;
	}

	*dst = '\0';
}

char* strndup(const char* src, size_t length)
{
    char* dst = malloc(length + 1);
    memcpy(dst, src, length);
    dst[length] = '\0';

    return dst;
}
