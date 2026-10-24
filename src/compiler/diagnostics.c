#include "compiler/diagnostics.h"
#include "common/errors.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

_Noreturn void fatalSourceError(int line, int column, const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    writeErrorMessage(format, arguments);
    va_end(arguments);
    fprintf(stderr, " on line %d:%d\n", line, column);
    exit(1);
}

_Noreturn void fatalTokenError(Token token, const char* suffix, const char* format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    writeErrorMessage(format, arguments);
    va_end(arguments);
    printTokenValue(token);
    fprintf(stderr, "%s on line %d:%d\n", suffix, token.line, token.column);
    exit(1);
}

_Noreturn void unexpectedEndError(Token token)
{
    fatalSourceError(token.line, token.column, "Unexpected end of input");
}
