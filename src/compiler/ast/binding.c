#include "compiler/ast/binding.h"
#include "compiler/ast/predicate.h"

bool has_no_value_reads(ASTNode* ast)
{
    return ast->variable_definition.read_count == ast->variable_definition.discarded_read_count;
}

void initialize_variable(ASTNode* ast)
{
    if (is_variable_definition(ast)) {
        ast->variable_definition.initialized = true;
        ast->variable_definition.possibly_initialized = true;
    }
}

bool is_initialized(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    if (ast->type == ASTVariableDefinition) {
        return ast->variable_definition.initialized;
    }

    return ast->type == ASTParameter;
}

bool is_moved(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.moved;
    }

    return symbol->variable_definition.moved;
}

void mark_address_taken(ASTNode* symbol)
{
    if (!symbol || !is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.address_taken = true;
}

void set_moved_state(ASTNode* symbol, bool moved)
{
    if (is_parameter(symbol)) {
        symbol->parameter.moved = moved;

        return;
    }

    symbol->variable_definition.moved = moved;
}

void track_variable_read(ASTNode* symbol)
{
    if (!is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.read_count++;
}
