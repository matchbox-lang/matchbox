#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void stripUnderscores(char *str, size_t *len);
char* strndup(const char *src, size_t len);
char* cleanNumberLiteral(char* src, size_t len);

#endif
