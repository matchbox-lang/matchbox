#ifndef AST_CONTROL_FLOW_H
#define AST_CONTROL_FLOW_H

#include "compiler/ast/ast.h"

ASTNode* find_node_return(ASTNode* ast);
ASTNode* get_branch_result(ASTNode* branch);
bool is_literal_match_pattern(ASTNode* pattern);
bool is_match_alternative(ASTNode* pattern);
bool is_simple_match_value(const ASTNode* ast);
bool match_has_wildcard(ASTNode* ast);

#endif
