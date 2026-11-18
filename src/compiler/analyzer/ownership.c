#include "compiler/analyzer/ownership.h"
#include "common/array_type.h"
#include "common/math.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/vector.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/lifetime.h"
#include "compiler/analyzer/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"

bool is_moved(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.moved;
    }

    return symbol->variable_definition.moved;
}

void set_moved_state(ASTNode* symbol, bool moved)
{
    if (is_parameter(symbol)) {
        symbol->parameter.moved = moved;

        return;
    }

    symbol->variable_definition.moved = moved;
}

static void consume_array_variable(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;
    bool moved = is_moved(symbol);
    bool exclusive = has_exclusive_access(symbol);
    size_t shared = *get_shared_access_count(symbol);

    if (moved || exclusive || shared) {
        semantic_error("Cannot move an unavailable array", ast->variable.token);
    }

    set_moved_state(symbol, true);
}

void consume_array(ASTNode* ast)
{
    Type type = get_node_data_type(ast);
    ReferenceType reference = get_reference_type(ast);

    if (!is_array_type(type.kind) || reference != RefNone) {
        return;
    }

    if (ast->type == ASTIndex) {
        semantic_error("Cannot move an array element out of its owner", ast->index.token);
    }

    if (ast->type == ASTVariable) {
        consume_array_variable(ast);
    }
}

static void update_ownership_state(OwnershipState* state)
{
    ASTNode* symbol = state->symbol;
    Vector* origins = get_reference_origins(symbol);

    copy_reference_origins(&state->reference_origins, origins);

    state->shared_access_count = *get_shared_access_count(symbol);
    state->exclusive_access_active = has_exclusive_access(symbol);
    state->initialized = is_initialized(symbol);
    state->possibly_initialized = state->initialized
        || (is_variable_definition(symbol) && symbol->variable_definition.possibly_initialized);
    state->moved = is_moved(symbol);
    state->reference_access_active = is_variable_definition(symbol)
        && symbol->variable_definition.reference_access_active;
}

static void capture_symbol_ownership(Vector* states, ASTNode* symbol)
{
    if (!is_variable_type(symbol)) {
        return;
    }

    OwnershipState* state = allocate_memory(sizeof(OwnershipState));
    state->symbol = symbol;

    init_vector(&state->reference_origins);
    update_ownership_state(state);
    push_vector_item(states, state);
}

static void capture_scope_ownership(Vector* states, Scope* scope)
{
    for (size_t i = 0; i < scope->symbols.capacity; i++) {
        TableItem* item = scope->symbols.data[i];

        while (item) {
            capture_symbol_ownership(states, item->value);
            item = item->next;
        }
    }
}

void capture_ownership_state(Vector* states, Scope* scope)
{
    init_vector(states);

    while (scope) {
        capture_scope_ownership(states, scope);
        scope = scope->parent;
    }
}

static void init_ownership_state_copy(OwnershipState* destination, OwnershipState* source)
{
    *destination = *source;

    init_vector(&destination->reference_origins);
    copy_reference_origins(&destination->reference_origins, &source->reference_origins);
}

void copy_ownership_state(Vector* destination, Vector* source)
{
    init_vector(destination);

    for (size_t i = 0; i < count_vector(source); i++) {
        OwnershipState* source_state = get_vector_at(source, i);
        OwnershipState* destination_state = allocate_memory(sizeof(OwnershipState));

        init_ownership_state_copy(destination_state, source_state);
        push_vector_item(destination, destination_state);
    }
}

static void apply_ownership_state(OwnershipState* state)
{
    ASTNode* symbol = state->symbol;

    *get_shared_access_count(symbol) = state->shared_access_count;
    set_exclusive_access(symbol, state->exclusive_access_active);
    set_moved_state(symbol, state->moved);

    if (!is_variable_definition(symbol)) {
        return;
    }

    Vector* origins = get_reference_origins(symbol);
    copy_reference_origins(origins, &state->reference_origins);
    symbol->variable_definition.initialized = state->initialized;
    symbol->variable_definition.possibly_initialized = state->possibly_initialized;
    symbol->variable_definition.reference_access_active = state->reference_access_active;
}

void apply_ownership_states(Vector* states)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        apply_ownership_state(get_vector_at(states, i));
    }
}

void update_ownership_states(Vector* states)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        OwnershipState* state = get_vector_at(states, i);

        update_ownership_state(state);
    }
}

void free_ownership_states(Vector* states)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        OwnershipState* state = get_vector_at(states, i);
        free_vector(&state->reference_origins);
        free_memory(state);
    }

    free_vector(states);
}

void release_references_dead_on_branch(Analyzer* analyzer, Vector* states, ASTNode* branch, ASTNode* binding)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        OwnershipState* state = get_vector_at(states, i);
        ASTNode* symbol = state->symbol;

        if (!is_variable_definition(symbol) || !symbol->variable_definition.reference_access_active) {
            continue;
        }

        StringObject* id = symbol->variable_definition.id;
        if (branch_uses_reference(branch, id, binding) || continuation_uses_reference(analyzer, id)) {
            continue;
        }

        release_reference_access(symbol);
    }
}

void analyze_ownership_branch(
    Analyzer* analyzer, Vector* states, ASTNode* branch, Type expected_type, Type* type, bool moves_result)
{
    apply_ownership_states(states);
    release_references_dead_on_branch(analyzer, states, branch, NULL);
    *type = analyze_else_branch(analyzer, branch, expected_type, moves_result);
    update_ownership_states(states);
}

static void merge_ownership_state(OwnershipState* then_state, OwnershipState* else_state)
{
    OwnershipState merged;

    init_ownership_state_copy(&merged, then_state);
    merge_reference_origins(&merged.reference_origins, &else_state->reference_origins);

    merged.shared_access_count = MAX(then_state->shared_access_count, else_state->shared_access_count);
    merged.exclusive_access_active = then_state->exclusive_access_active || else_state->exclusive_access_active;
    merged.initialized = then_state->initialized && else_state->initialized;
    merged.possibly_initialized = then_state->possibly_initialized || else_state->possibly_initialized;
    merged.moved = then_state->moved || else_state->moved;
    merged.reference_access_active = then_state->reference_access_active || else_state->reference_access_active;

    apply_ownership_state(&merged);
    free_vector(&merged.reference_origins);
}

void merge_ownership_states(Vector* then_states, Vector* else_states)
{
    size_t count = count_vector(then_states);

    for (size_t i = 0; i < count; i++) {
        merge_ownership_state(get_vector_at(then_states, i), get_vector_at(else_states, i));
    }
}

static void validate_loop_reference_origins(OwnershipState* entry, Token token)
{
    Vector* origins = get_reference_origins(entry->symbol);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);
        bool existing = contains_reference_origin(&entry->reference_origins, origin);

        if (!existing) {
            semantic_error("While loop cannot change the origin of an outer reference", token);
        }
    }
}

static void validate_loop_initialization(OwnershipState* entry, Token token)
{
    ASTNode* symbol = entry->symbol;

    if (!is_variable_definition(symbol) || !symbol->variable_definition.fixed || entry->possibly_initialized) {
        return;
    }

    if (symbol->variable_definition.possibly_initialized) {
        semantic_error("While loop cannot initialize a fixed outer binding across iterations", token);
    }
}

void validate_loop_ownership(Vector* entry_states, Token token)
{
    size_t count = count_vector(entry_states);

    for (size_t i = 0; i < count; i++) {
        OwnershipState* entry = get_vector_at(entry_states, i);
        ASTNode* symbol = entry->symbol;
        bool moved = is_moved(symbol);

        if (moved && !entry->moved) {
            semantic_error("While loop cannot move an outer reference across iterations", token);
        }

        validate_loop_reference_origins(entry, token);
        validate_loop_initialization(entry, token);
    }
}

void merge_ownership_outcome(Vector* merged_states, Vector* states, bool* initialized)
{
    if (!*initialized) {
        copy_ownership_state(merged_states, states);
        *initialized = true;

        return;
    }

    merge_ownership_states(merged_states, states);
    update_ownership_states(merged_states);
}

static void record_function_array_initialization(ASTNode* function, OwnershipState* state)
{
    ASTNode* symbol = state->symbol;
    bool initialized = is_initialized(symbol);
    bool possible = symbol->variable_definition.possibly_initialized;
    bool uninitialized_return = contains_binding(
        &function->function_definition.array_uninitialized_returns, symbol);

    if (!state->initialized && initialized && !uninitialized_return) {
        push_vector_item(&function->function_definition.array_initializations, symbol);
    }

    if (!state->possibly_initialized && possible) {
        push_vector_item(&function->function_definition.array_possible_initializations, symbol);
    }

    symbol->variable_definition.initialized = state->initialized;
    symbol->variable_definition.possibly_initialized = state->possibly_initialized;
}

static void record_function_array_move(ASTNode* function, OwnershipState* state)
{
    ASTNode* symbol = state->symbol;
    Scope* scope = get_scope(symbol);
    Type type = get_node_data_type(symbol);

    if (!is_top_level_scope(scope) || !is_array_type(type.kind)) {
        return;
    }

    bool moved = is_moved(symbol);

    if (moved && !state->moved) {
        push_vector_item(&function->function_definition.array_moves, symbol);
    }

    set_moved_state(symbol, state->moved);
    record_function_array_initialization(function, state);
}

void record_function_array_moves(ASTNode* function, Vector* states)
{
    for (size_t i = 0; i < states->count; i++) {
        record_function_array_move(function, states->data[i]);
    }

    free_ownership_states(states);
}

static void record_uninitialized_array_return(ASTNode* function, OwnershipState* state)
{
    ASTNode* symbol = state->symbol;
    Scope* scope = get_scope(symbol);
    Type type = get_node_data_type(symbol);

    if (!is_top_level_scope(scope) || !is_array_type(type.kind) || state->initialized) {
        return;
    }

    push_vector_item(&function->function_definition.array_uninitialized_returns, symbol);
}

void record_array_return_initializations(Analyzer* analyzer)
{
    Vector states;
    capture_ownership_state(&states, analyzer->current_scope);

    for (size_t i = 0; i < states.count; i++) {
        record_uninitialized_array_return(analyzer->function, states.data[i]);
    }

    free_ownership_states(&states);
}
