#ifndef ERRORS_H
#define ERRORS_H

#include <stdarg.h>

void writeErrorMessage(const char* format, va_list arguments);
void reportError(const char* format, ...);
_Noreturn void fatalError(const char* format, ...);
_Noreturn void outOfMemoryError(void);

#endif
