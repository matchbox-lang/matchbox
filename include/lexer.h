#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct Position
{
    char* chars;
    int line;
    int column;
} Position;

typedef struct Lexer
{
    Position current;
    Position start;
} Lexer;

void initLexer(Lexer* lexer, char* source);
Token scanToken(Lexer* lexer);

#endif
