#include "util.h"
#include <stdlib.h>
#include <string.h>

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

char* dupnstr(const char *src, size_t len)
{
    char* dst = malloc(len + 1);
    memcpy(dst, src, len);
    dst[len] = '\0';

    return dst;
}
