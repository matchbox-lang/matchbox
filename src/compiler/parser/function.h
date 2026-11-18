#ifndef PARSER_FUNCTION_H
#define PARSER_FUNCTION_H

#include "compiler/parser/parser.h"

ASTNode* parse_return_statement(Parser* parser);
bool parse_argument_list(Parser* parser, Vector* args);
ASTNode* parse_function_call(Parser* parser);
bool parse_function_body(Parser* parser, ASTNode* ast);
ASTNode* parse_function_definition(Parser* parser);

#endif
