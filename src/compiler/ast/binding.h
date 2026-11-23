#ifndef AST_BINDING_H
#define AST_BINDING_H

#include "compiler/ast/ast.h"

bool has_no_value_reads(ASTNode* ast);
void initialize_variable(ASTNode* ast);
bool is_initialized(const ASTNode* ast);
bool is_moved(ASTNode* symbol);
void mark_address_taken(ASTNode* symbol);
void set_moved_state(ASTNode* symbol, bool moved);
void track_variable_read(ASTNode* symbol);

#endif
