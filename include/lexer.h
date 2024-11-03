#ifndef LEXER_H
#define LEXER_H

#include "token.h"

void initLexer(char* source);
Token scanToken();

#endif
