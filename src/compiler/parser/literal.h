#ifndef PARSER_LITERAL_H
#define PARSER_LITERAL_H

#include "compiler/parser/parser.h"

TypeKind infer_integer_literal_type(uint64_t value, bool negative);
ASTNode* parse_boolean_literal_node(Parser* parser, Token token);
ASTNode* parse_integer_literal_node(Parser* parser, Token token);
ASTNode* parse_float_literal_node(Parser* parser, Token token);
ASTNode* parse_binary_literal_node(Parser* parser, Token token);
ASTNode* parse_hexadecimal_literal_node(Parser* parser, Token token);
ASTNode* parse_octal_literal_node(Parser* parser, Token token);
ASTNode* parse_character_literal(Parser* parser);

#endif
