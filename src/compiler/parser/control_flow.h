#ifndef PARSER_CONTROL_FLOW_H
#define PARSER_CONTROL_FLOW_H

#include "compiler/parser/parser.h"

bool parse_branch_statement(Parser* parser, ASTNode* branch, bool expression);
ASTNode* parse_conditional(Parser* parser, bool expression);
ASTNode* parse_while_statement(Parser* parser);

#endif
