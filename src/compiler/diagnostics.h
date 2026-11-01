#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "compiler/token.h"

_Noreturn void fatal_source_error(int line, int column, const char* format, ...);
_Noreturn void fatal_token_error(Token token, const char* suffix, const char* format, ...);
_Noreturn void unexpected_end_of_input_error(Token token);

#endif
