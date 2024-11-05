#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct Position
{
    char* chars;
    int line;
    int column;
} Position;

void initLexer(char* source);
Token scanToken();

#endif
