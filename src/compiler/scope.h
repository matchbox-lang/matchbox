#ifndef SCOPE_H
#define SCOPE_H

#include "common/table.h"

typedef struct ASTNode ASTNode;
typedef struct Scope Scope;

typedef struct Scope
{
    Scope* parent;
    Scope* frame_scope;
    size_t local_offset;
    size_t local_count;
    size_t max_local_count;
    size_t level;
    Table symbols;
} Scope;

Scope* create_scope(Scope* parent);
void free_scope(Scope* scope);
size_t get_max_local_count(const Scope* scope);
size_t get_next_local_position(const Scope* scope);
size_t get_scope_level(const Scope* scope);
bool is_top_level_scope(const Scope* scope);
ASTNode* set_local_symbol(Scope* scope, StringView id, ASTNode* symbol);
ASTNode* set_local_variable_symbol(Scope* scope, StringView id, ASTNode* symbol);
ASTNode* get_local_symbol(const Scope* scope, StringView id);
ASTNode* get_symbol(const Scope* scope, StringView id);

#endif
