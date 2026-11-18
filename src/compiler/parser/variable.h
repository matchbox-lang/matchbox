#ifndef PARSER_VARIABLE_H
#define PARSER_VARIABLE_H

#include "compiler/parser/parser.h"

ASTNode* parse_variable_definition(Parser* parser);
ASTNode* parse_identifier(Parser* parser);
ASTNode* parse_constant_statement(Parser* parser);

#endif
