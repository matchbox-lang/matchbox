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

Vector* get_tuple_moves(ASTNode* binding)
{
    if (is_parameter(binding)) {
        return &binding->parameter.tuple_moves;
    }

    return &binding->variable_definition.tuple_moves;
}

static bool tuple_move_in_range(const void* move, size_t start, size_t slots)
{
    size_t position = (uintptr_t)move - 1;

    return position >= start && position - start < slots;
}

bool tuple_moves_overlap(const Vector* moves, size_t start, size_t slots)
{
    size_t index = 0;

    while (index < moves->count && !tuple_move_in_range(moves->data[index], start, slots)) {
        index++;
    }

    return index < moves->count;
}

void copy_tuple_moves(Vector* destination, const Vector* source)
{
    if (destination == source) {
        return;
    }

    resize_vector(destination, source->count);

    for (size_t i = 0; i < source->count; i++) {
        destination->data[i] = source->data[i];
    }
}

static void append_tuple_move(Vector* destination, const void* move)
{
    size_t position = (uintptr_t)move - 1;
    bool existing = tuple_moves_overlap(destination, position, 1);

    if (!existing) {
        push_vector_item(destination, (void*)move);
    }
}

void merge_tuple_moves(Vector* destination, const Vector* source)
{
    for (size_t i = 0; i < source->count; i++) {
        append_tuple_move(destination, source->data[i]);
    }
}

void record_tuple_move(ASTNode* binding, size_t start, size_t slots)
{
    Vector* moves = get_tuple_moves(binding);

    for (size_t i = 0; i < slots; i++) {
        push_vector_item(moves, (void*)(uintptr_t)(start + i + 1));
    }

    mark_address_taken(binding);
}

static void retain_tuple_move(Vector* moves, size_t index, size_t* count, size_t start, size_t slots)
{
    if (tuple_move_in_range(moves->data[index], start, slots)) {
        return;
    }

    moves->data[(*count)++] = moves->data[index];
}

void forget_tuple_moves(ASTNode* binding, size_t start, size_t slots)
{
    Vector* moves = get_tuple_moves(binding);
    size_t count = 0;

    for (size_t i = 0; i < moves->count; i++) {
        retain_tuple_move(moves, i, &count, start, slots);
    }

    moves->count = count;
}

static bool includes_tuple_move(const Vector* destination, const void* move)
{
    size_t position = (uintptr_t)move - 1;

    return tuple_moves_overlap(destination, position, 1);
}

bool tuple_moves_include(const Vector* destination, const Vector* source)
{
    size_t index = 0;

    while (index < source->count && includes_tuple_move(destination, source->data[index])) {
        index++;
    }

    return index == source->count;
}
