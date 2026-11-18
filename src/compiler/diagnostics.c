#include "compiler/diagnostics.h"
#include "common/errors.h"
#include <stdarg.h>
#include <stdio.h>

_Noreturn void fatal_source_error(int line, int column, const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    write_error_message(format, arguments);
    va_end(arguments);
    fprintf(stderr, " on line %d:%d\n", line, column);
    raise_error();
}

_Noreturn void fatal_token_error(Token token, const char* suffix, const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    write_error_message(format, arguments);
    va_end(arguments);
    print_token_value(token);
    fprintf(stderr, "%s on line %d:%d\n", suffix, token.line, token.column);
    raise_error();
}

_Noreturn void unexpected_end_of_input_error(Token token)
{
    fatal_source_error(token.line, token.column, "Unexpected end of input");
}

_Noreturn void binding_access_error(char* message, Token token, char* detail)
{
    fatal_token_error(token, detail, "%s", message);
}

_Noreturn void redefinition_error(char* kind, Token token)
{
    fatal_token_error(token, " is already defined", "%s ", kind);
}

_Noreturn void semantic_error(char* message, Token token)
{
    fatal_source_error(token.line, token.column, "%s", message);
}

_Noreturn void semantic_token_error(char* message, Token token)
{
    fatal_token_error(token, "", "%s", message);
}

_Noreturn void symbol_error(const char* detail, Token token)
{
    fatal_token_error(token, detail, "");
}
