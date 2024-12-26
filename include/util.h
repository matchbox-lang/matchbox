#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void stripUnderscores(char *str, size_t *len);
char* dupnstr(const char *str, size_t len);

#endif
