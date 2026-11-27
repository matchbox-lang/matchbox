#ifndef PARSER_GENERIC_H
#define PARSER_GENERIC_H

#include "compiler/parser/parser.h"

bool is_type_argument_list(Parser* parser);
Type parse_named_type(Parser* parser, Token token, ReferenceType reference_type, bool* complete);
bool parse_generic_type_names(Parser* parser, Vector* names);
bool parse_generic_names(Parser* parser, ASTNode* ast);
bool parse_type_arguments(Parser* parser, Vector* types);

#endif
