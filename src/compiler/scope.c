#include "compiler/scope.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/util.h"
#include "compiler/ast.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static Scope* get_frame_scope(Scope* scope, Scope* parent)
{
    if (parent) {
        return parent->frame_scope;
    }

    return scope;
}

static size_t get_local_offset(const Scope* parent)
{
    if (!parent) {
        return 0;
    }

    return parent->local_offset + parent->local_count;
}

Scope* create_scope(Scope* parent)
{
    Scope* scope = allocate_memory(sizeof(Scope));
    scope->parent = parent;
    scope->frame_scope = get_frame_scope(scope, parent);
    scope->local_offset = get_local_offset(parent);
    scope->local_count = 0;
    scope->max_local_count = scope->local_offset;
    scope->level = get_scope_level(parent) + 1;

    init_table(&scope->symbols, 32);
    
    return scope;
}

void free_scope(Scope* scope)
{
    if (!scope) {
        return;
    }

    free_table(&scope->symbols);
    free_memory(scope);
}

static size_t get_symbol_slot_count(const ASTNode* symbol)
{
    if (!is_variable_type(symbol)) {
        return 0;
    }

    if (get_reference_type(symbol) != RefNone) {
        return 1;
    }

    return get_type_slot_count(get_node_data_type(symbol));
}

size_t get_max_local_count(const Scope* scope)
{
    return scope->frame_scope->max_local_count;
}

size_t get_next_local_position(const Scope* scope)
{
    return scope->local_offset + scope->local_count;
}

size_t get_scope_level(const Scope* scope)
{
    return scope ? scope->level : 0;
}

bool is_top_level_scope(const Scope* scope)
{
    return scope->level == 1;
}

ASTNode* set_local_symbol(Scope* scope, StringView id, ASTNode* symbol)
{
    if (set_table_at(&scope->symbols, id, symbol)) {
        return symbol;
    }

    return NULL;
}

ASTNode* set_local_variable_symbol(Scope* scope, StringView id, ASTNode* symbol)
{
    scope->local_count += get_symbol_slot_count(symbol);
    size_t local_extent = get_next_local_position(scope);

    scope->frame_scope->max_local_count = MAX(scope->frame_scope->max_local_count, local_extent);
    
    return set_local_symbol(scope, id, symbol);
}

ASTNode* get_local_symbol(const Scope* scope, StringView id)
{
    return get_table_at(&scope->symbols, id);
}

ASTNode* get_symbol(const Scope* scope, StringView id)
{
    ASTNode* symbol = get_table_at(&scope->symbols, id);

    if (symbol) {
        return symbol;
    }

    if (scope->parent) {
        return get_symbol(scope->parent, id);
    }

    return NULL;
}
