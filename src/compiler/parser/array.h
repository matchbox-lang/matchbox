#ifndef PARSER_ARRAY_H
#define PARSER_ARRAY_H

#include "compiler/parser/parser.h"

ASTNode* parse_array_initializer(Parser* parser, Token token, TypeKind kind);
ASTNode* parse_array(Parser* parser);
ASTNode* parse_index(Parser* parser, ASTNode* array);

#endif
