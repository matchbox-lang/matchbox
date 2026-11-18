#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "compiler/lexer/token.h"

_Noreturn void fatal_source_error(int line, int column, const char* format, ...);
_Noreturn void fatal_token_error(Token token, const char* suffix, const char* format, ...);
_Noreturn void unexpected_end_of_input_error(Token token);

_Noreturn void binding_access_error(char* message, Token token, char* detail);
_Noreturn void redefinition_error(char* kind, Token token);
_Noreturn void semantic_error(char* message, Token token);
_Noreturn void semantic_token_error(char* message, Token token);
_Noreturn void symbol_error(const char* detail, Token token);

#endif
