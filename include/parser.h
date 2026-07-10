#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include "scope.h"
#include "token.h"

typedef struct Parser
{
    Lexer lexer;
    Token currentToken;
    Token prevToken;
    Scope* currentScope;
    AST* topLevel;
} Parser;

void initParser(Parser* parser, AST* ast);
bool parse(Parser* parser, char* source);

#endif
