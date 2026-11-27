#include "compiler/analyzer/ownership.h"
#include "common/array_type.h"
#include "common/math.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/tuple_type.h"
#include "common/vector.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/lifetime.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/function.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"

static void apply_binding_move_effects(ASTNode* symbol, const FunctionArrayEffects* effects)
{
    bool active = is_variable_definition(symbol) && symbol->variable_definition.reference_access_active;

    if (active) {
        release_reference_access(symbol);
    }

    bool partial = apply_tuple_move_effects(symbol, effects);

    if (!partial) {
        set_moved_state(symbol, true);

        return;
    }

    if (active) {
        activate_tuple_access(symbol);
    }
}

static void apply_function_array_moves(ASTNode* function)
{
    FunctionArrayEffects* effects = function->function_definition.array_effects;
    if (!effects) {
        return;
    }

    Vector* moves = &effects->moves;

    for (size_t i = 0; i < moves->count; i++) {
        apply_binding_move_effects(moves->data[i], effects);
    }
}

static void apply_function_possible_initializations(ASTNode* function)
{
    FunctionArrayEffects* effects = function->function_definition.array_effects;
    if (!effects) {
        return;
    }

    Vector* bindings = &effects->possible_initializations;

    for (size_t i = 0; i < bindings->count; i++) {
        ASTNode* symbol = bindings->data[i];
        symbol->variable_definition.possibly_initialized = true;
    }
}

static void apply_function_array_initializations(ASTNode* function)
{
    FunctionArrayEffects* effects = function->function_definition.array_effects;
    if (!effects) {
        return;
    }

    Vector* bindings = &effects->initializations;

    apply_function_possible_initializations(function);

    for (size_t i = 0; i < bindings->count; i++) {
        initialize_variable(bindings->data[i]);
    }
}

void apply_function_ownership_effects(ASTNode* function)
{
    apply_function_array_moves(function);
    apply_function_array_initializations(function);
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

    if (symbol->tuple_references.count && is_variable_definition(symbol)
        && symbol->variable_definition.reference_access_active) {
        release_reference_access(symbol);
    }
}

static void consume_index_owner(ASTNode* ast)
{
    bool consumed = consume_tuple_element(ast);

    if (!consumed) {
        semantic_error("Cannot move an array element out of its owner", ast->index.token);
    }
}

void consume_array(ASTNode* ast)
{
    Type type = get_node_data_type(ast);
    ReferenceType reference = get_reference_type(ast);

    if (!is_owned_type(type) || reference != RefNone) {
        return;
    }

    if (ast->type == ASTIndex) {
        consume_index_owner(ast);
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
    copy_tuple_reference_vector(&state->tuple_references, &symbol->tuple_references);

    Vector* moves = get_tuple_moves(symbol);

    copy_tuple_moves(&state->tuple_moves, moves);

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
    init_vector(&state->tuple_references);
    init_vector(&state->tuple_moves);
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

void capture_ownership_states(Vector* states, Scope* scope)
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
    init_vector(&destination->tuple_references);
    init_vector(&destination->tuple_moves);
    copy_tuple_moves(&destination->tuple_moves, &source->tuple_moves);
    copy_tuple_reference_vector(&destination->tuple_references, &source->tuple_references);
    copy_reference_origins(&destination->reference_origins, &source->reference_origins);
}

void copy_ownership_states(Vector* destination, Vector* source)
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

    copy_tuple_reference_vector(&symbol->tuple_references, &state->tuple_references);

    Vector* moves = get_tuple_moves(symbol);

    copy_tuple_moves(moves, &state->tuple_moves);

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
        free_tuple_reference_vector(&state->tuple_references);
        free_vector(&state->tuple_moves);
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
    merge_tuple_reference_vectors(&merged.tuple_references, &else_state->tuple_references);
    merge_tuple_moves(&merged.tuple_moves, &else_state->tuple_moves);

    merged.shared_access_count = MAX(then_state->shared_access_count, else_state->shared_access_count);
    merged.exclusive_access_active = then_state->exclusive_access_active || else_state->exclusive_access_active;
    merged.initialized = then_state->initialized && else_state->initialized;
    merged.possibly_initialized = then_state->possibly_initialized || else_state->possibly_initialized;
    merged.moved = then_state->moved || else_state->moved;
    merged.reference_access_active = then_state->reference_access_active || else_state->reference_access_active;

    apply_ownership_state(&merged);
    free_vector(&merged.reference_origins);
    free_tuple_reference_vector(&merged.tuple_references);
    free_vector(&merged.tuple_moves);
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
            semantic_error("Cannot change origin of an outer reference", token);
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
        semantic_error("Cannot initialize fixed outer binding across iterations", token);
    }
}

static void validate_loop_tuple_moves(OwnershipState* entry, Token token)
{
    Vector* moves = get_tuple_moves(entry->symbol);
    bool unchanged = tuple_moves_include(&entry->tuple_moves, moves);

    if (!unchanged) {
        semantic_error("Cannot move an outer tuple element across iterations", token);
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
            semantic_error("Cannot move outer reference across iterations", token);
        }

        validate_loop_tuple_moves(entry, token);
        validate_loop_reference_origins(entry, token);
        validate_loop_initialization(entry, token);
    }
}

void merge_ownership_outcome(Vector* merged_states, Vector* states, bool* initialized)
{
    if (!*initialized) {
        copy_ownership_states(merged_states, states);
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
    bool uninitialized_return = false;

    FunctionArrayEffects* effects = function->function_definition.array_effects;
    if (effects) {
        uninitialized_return = contains_binding(&effects->uninitialized_returns, symbol);
    }

    bool initializes = !state->initialized && initialized && !uninitialized_return;
    bool possibly_initializes = !state->possibly_initialized && possible;
    symbol->variable_definition.initialized = state->initialized;
    symbol->variable_definition.possibly_initialized = state->possibly_initialized;

    if (!initializes && !possibly_initializes) {
        return;
    }

    if (!effects) {
        effects = create_function_array_effects(function);
    }

    if (initializes) {
        push_vector_item(&effects->initializations, symbol);
    }

    if (possibly_initializes) {
        push_vector_item(&effects->possible_initializations, symbol);
    }
}

static void record_tuple_slot_move(ASTNode* function, OwnershipState* state, void* move)
{
    size_t offset = (uintptr_t)move - 1;
    bool existing = tuple_moves_overlap(&state->tuple_moves, offset, 1);

    if (!existing) {
        add_function_tuple_move(function, state->symbol, offset, false);
    }
}

static void record_tuple_slot_restoration(ASTNode* function, OwnershipState* state, void* move)
{
    size_t offset = (uintptr_t)move - 1;
    Vector* moves = get_tuple_moves(state->symbol);
    bool moved = tuple_moves_overlap(moves, offset, 1);

    if (!moved) {
        add_function_tuple_move(function, state->symbol, offset, true);
    }
}

static void record_tuple_move_effects(ASTNode* function, OwnershipState* state)
{
    if (is_moved(state->symbol)) {
        return;
    }

    Vector* moves = get_tuple_moves(state->symbol);

    for (size_t i = 0; i < moves->count; i++) {
        record_tuple_slot_move(function, state, moves->data[i]);
    }

    for (size_t i = 0; i < state->tuple_moves.count; i++) {
        record_tuple_slot_restoration(function, state, state->tuple_moves.data[i]);
    }
}

static void record_array_move_effect(ASTNode* function, OwnershipState* state)
{
    Vector* moves = get_tuple_moves(state->symbol);
    bool unchanged = tuple_moves_include(&state->tuple_moves, moves);
    bool restored = !tuple_moves_include(moves, &state->tuple_moves);
    bool moved = is_moved(state->symbol) || !unchanged || restored;

    if (!moved || state->moved) {
        return;
    }

    FunctionArrayEffects* effects = function->function_definition.array_effects;
    if (!effects) {
        effects = create_function_array_effects(function);
    }

    push_vector_item(&effects->moves, state->symbol);
    record_tuple_move_effects(function, state);
}

static void record_function_array_move(ASTNode* function, OwnershipState* state)
{
    ASTNode* symbol = state->symbol;
    Scope* scope = get_scope(symbol);
    Type type = get_node_data_type(symbol);

    if (!is_top_level_scope(scope) || !is_owned_type(type)) {
        return;
    }

    record_array_move_effect(function, state);
    set_moved_state(symbol, state->moved);

    Vector* moves = get_tuple_moves(symbol);

    copy_tuple_moves(moves, &state->tuple_moves);
    record_function_array_initialization(function, state);
}

static void restore_tuple_origin_state(Vector* states, ASTNode* origin)
{
    size_t index = 0;

    while (index < states->count && ((OwnershipState*)states->data[index])->symbol != origin) {
        index++;
    }

    if (index == states->count) {
        return;
    }

    OwnershipState* state = states->data[index];
    *get_shared_access_count(origin) = state->shared_access_count;

    set_exclusive_access(origin, state->exclusive_access_active);
}

static void restore_function_tuple_references(OwnershipState* state, Vector* states)
{
    ASTNode* symbol = state->symbol;
    Type type = get_node_data_type(symbol);

    if (type.kind != TypeTuple || !is_variable_definition(symbol)) {
        return;
    }

    copy_tuple_reference_vector(&symbol->tuple_references, &state->tuple_references);
    symbol->variable_definition.reference_access_active = state->reference_access_active;

    for (size_t i = 0; i < state->tuple_references.count; i++) {
        TupleReference* access = state->tuple_references.data[i];

        restore_tuple_origin_state(states, access->origin);
    }
}

void record_function_array_moves(ASTNode* function, Vector* states)
{
    for (size_t i = 0; i < states->count; i++) {
        record_function_array_move(function, states->data[i]);
    }

    for (size_t i = 0; i < states->count; i++) {
        restore_function_tuple_references(states->data[i], states);
    }

    free_ownership_states(states);
}

static void record_uninitialized_array_return(ASTNode* function, OwnershipState* state)
{
    ASTNode* symbol = state->symbol;
    Scope* scope = get_scope(symbol);
    Type type = get_node_data_type(symbol);

    if (!is_top_level_scope(scope) || !is_owned_type(type) || state->initialized) {
        return;
    }

    FunctionArrayEffects* effects = function->function_definition.array_effects;
    if (!effects) {
        effects = create_function_array_effects(function);
    }

    push_vector_item(&effects->uninitialized_returns, symbol);
}

void record_array_return_initializations(Analyzer* analyzer)
{
    Vector states;
    capture_ownership_states(&states, analyzer->current_scope);

    for (size_t i = 0; i < states.count; i++) {
        record_uninitialized_array_return(analyzer->function, states.data[i]);
    }

    free_ownership_states(&states);
}
