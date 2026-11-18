#ifndef PARSER_TYPE_H
#define PARSER_TYPE_H

#include "compiler/parser/parser.h"

void consume_type(Parser* parser);
ReferenceType parse_reference_type(Parser* parser);
ASTNode* parse_type_expression(Parser* parser);
void parse_dimensions(Parser* parser, Type* type);
void parse_function_return_type(Parser* parser, ASTNode* ast);
void parse_variable_type(Parser* parser, ASTNode* ast);

#endif
