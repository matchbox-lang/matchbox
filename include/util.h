#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>

bool isLargerThan8BitSigned(int n);
bool isLargerThan16BitSigned(int n);
void stripUnderscores(char *str, size_t *len);
char* strndup(const char *src, size_t len);

#endif
