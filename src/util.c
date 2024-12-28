#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void stripUnderscores(char *str, size_t *len)
{
	char *src = str;
    char *dst = str;

	while (*src != '\0') {
		if (*src != '_') {
			*dst = *src;
			dst++;
		} else {
			--(*len);
		}

		src++;
	}

	*dst = '\0';
}

char* strndup(const char *src, size_t len)
{
    char* dst = malloc(len + 1);
    memcpy(dst, src, len);
    dst[len] = '\0';

    return dst;
}

char* cleanNumberLiteral(char* src, size_t len)
{
    char* str = strndup(src, len);
    stripUnderscores(str, &len);

    return str;
}
