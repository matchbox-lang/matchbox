#ifndef AST_QUERY_H
#define AST_QUERY_H

#include "compiler/ast/ast.h"

ASTNode* get_call_parameter(Vector* parameters, size_t position);
ASTNode* get_index_constant_expression(ASTNode* ast);
Type get_node_data_type(const ASTNode* ast);
ReferenceType get_parameter_argument_reference(ASTNode* parameter);
ASTNode* get_root_binding(const ASTNode* ast);
Scope* get_scope(const ASTNode* ast);

#endif
