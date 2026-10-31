#ifndef PARSER_H
#define PARSER_H

#include "compiler/ast.h"
#include "compiler/lexer.h"
#include "compiler/token.h"

typedef struct Parser
{
    ASTNode* topLevel;
    Lexer lexer;
    Token currentToken;
    Token previousToken;
} Parser;

void initParser(Parser* parser, ASTNode* ast);
bool parse(Parser* parser, const char* source);

#endif
