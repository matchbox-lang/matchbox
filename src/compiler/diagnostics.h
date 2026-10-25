#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "compiler/token.h"

_Noreturn void fatalSourceError(int line, int column, const char* format, ...);
_Noreturn void fatalTokenError(Token token, const char* suffix, const char* format, ...);
_Noreturn void unexpectedEndOfInputError(Token token);

#endif
