#ifndef LEXER_H
#define LEXER_H

#include "compiler/source_position.h"
#include "compiler/token.h"

typedef struct Lexer
{
    SourcePosition current;
    SourcePosition start;
} Lexer;

void init_lexer(Lexer* lexer, const char* source);
Token scan_token(Lexer* lexer);

#endif
