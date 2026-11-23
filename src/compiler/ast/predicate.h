#ifndef AST_PREDICATE_H
#define AST_PREDICATE_H

#include "compiler/ast/ast.h"

bool is_constant_index(const ASTNode* ast);
bool is_expression_statement(const ASTNode* ast);
bool is_function_call(const ASTNode* ast);
bool is_function_definition(const ASTNode* ast);
bool is_index(const ASTNode* ast);
bool is_none(const ASTNode* ast);
bool is_parameter(const ASTNode* ast);
bool is_prefix(const ASTNode* ast);
bool is_prefix_operand(const ASTNode* ast);
bool is_variable(const ASTNode* ast);
bool is_variable_definition(const ASTNode* ast);
bool is_variable_type(const ASTNode* ast);

#endif
