#ifndef PARSER_EXPRESSION_H
#define PARSER_EXPRESSION_H

#include "compiler/parser/parser.h"

ASTNode* parse_group_expression(Parser* parser);
ASTNode* parse_expression(Parser* parser);

#endif
