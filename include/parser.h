#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include "token.h"

typedef struct Parser
{
    ASTNode* topLevel;
    Lexer lexer;
    Token currentToken;
    Token previousToken;
} Parser;

void initParser(Parser* parser, ASTNode* ast);
bool parse(Parser* parser, char* source);

#endif
