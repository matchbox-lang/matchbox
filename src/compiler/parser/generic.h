#ifndef PARSER_GENERIC_H
#define PARSER_GENERIC_H

#include "compiler/parser/parser.h"

bool is_type_argument_list(Parser* parser);
Type parse_named_type(Parser* parser, Token token);
bool parse_generic_names(Parser* parser, ASTNode* ast);
bool parse_type_arguments(Parser* parser, Vector* types);

#endif
