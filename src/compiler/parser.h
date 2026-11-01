#ifndef PARSER_H
#define PARSER_H

#include "compiler/ast.h"
#include "compiler/lexer.h"
#include "compiler/token.h"

typedef struct Parser
{
    ASTNode* top_level;
    Lexer lexer;
    Token current_token;
    Token previous_token;
} Parser;

void init_parser(Parser* parser, ASTNode* ast);
bool parse(Parser* parser, const char* source);

#endif
