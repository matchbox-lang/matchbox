#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include "token.h"

typedef struct Parser
{
    AST* topLevel;
    Lexer lexer;
    Token currentToken;
    Token prevToken;
} Parser;

void initParser(Parser* parser, AST* ast);
bool parse(Parser* parser, char* source);

#endif
