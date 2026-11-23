#ifndef AST_LITERAL_H
#define AST_LITERAL_H

#include "compiler/ast/ast.h"

void copy_flattened_array_literals(ASTNode* ast, Vector* literals);
ASTNode* copy_literal_node(const ASTNode* literal);
ASTNode* get_float_literal(ASTNode* expression);
ASTNode* get_integer_literal(ASTNode* expression, bool* negative);
bool is_literal(const ASTNode* ast);
bool is_literal_array_tree(ASTNode* ast);
bool is_literal_of_type(ASTNode* ast, Type type);
bool is_numeric_literal(ASTNode* argument);
bool is_text_literal(const ASTNode* ast);
void set_literal_token(ASTNode* ast, Token token);

#endif
