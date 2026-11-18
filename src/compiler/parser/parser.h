#ifndef COMPILER_PARSER_H
#define COMPILER_PARSER_H

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

_Noreturn void expected_expression_error(Token token);
_Noreturn void expected_token_error(TokenType type, Token token);
void advance(Parser* parser);
void consume(Parser* parser, TokenType type);
bool is_end_of_file(Parser* parser);
ASTNode* parse_statement(Parser* parser);
bool parse_blocklevel_statements(Parser* parser, Vector* nodes);
void init_parser(Parser* parser, ASTNode* ast);
bool parse(Parser* parser, const char* source);

#endif
