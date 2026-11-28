#include "compiler/analyzer/tuple_reference.h"
#include "common/memory.h"
#include "common/string_view.h"
#include "compiler/ast/reference.h"

typedef struct TupleReferenceState
{
    ASTNode* node;
    Vector references;
    bool persistent;
} TupleReferenceState;

static TupleReferenceState* find_tuple_reference_state(Analyzer* analyzer, ASTNode* ast)
{
    StringView key = create_string_view((const char*)&ast, sizeof(ast));

    return get_table_at(&analyzer->tuple_references, key);
}

static Vector* create_tuple_references(Analyzer* analyzer, ASTNode* ast)
{
    TupleReferenceState* state = find_tuple_reference_state(analyzer, ast);

    if (state) {
        return &state->references;
    }

    state = allocate_zeroed_memory(1, sizeof(TupleReferenceState));
    state->node = ast;
    state->persistent = ast->type == ASTVariableDefinition || ast->type == ASTParameter
        || ast->type == ASTFunctionDefinition;
    StringView key = create_string_view((const char*)&state->node, sizeof(state->node));

    set_table_at(&analyzer->tuple_references, key, state);

    return &state->references;
}

const Vector* get_tuple_references(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->type == ASTVariable) {
        ast = ast->variable.symbol;
    }

    TupleReferenceState* state = find_tuple_reference_state(analyzer, ast);

    if (state) {
        return &state->references;
    }

    static const Vector empty;

    return &empty;
}

void free_tuple_reference_vector(Vector* references)
{
    for (size_t i = 0; i < references->count; i++) {
        free_memory(references->data[i]);
    }

    free_vector(references);
    init_vector(references);
}

void clear_tuple_references(Analyzer* analyzer, ASTNode* ast)
{
    TupleReferenceState* state = find_tuple_reference_state(analyzer, ast);

    if (!state) {
        return;
    }

    StringView key = create_string_view((const char*)&state->node, sizeof(state->node));

    delete_table_at(&analyzer->tuple_references, key);
    free_tuple_reference_vector(&state->references);
    free_memory(state);
}

void copy_tuple_reference_vector(Vector* destination, const Vector* source)
{
    if (destination == source) {
        return;
    }

    free_tuple_reference_vector(destination);

    for (size_t i = 0; i < source->count; i++) {
        TupleReference* copy = allocate_memory(sizeof(TupleReference));
        *copy = *(TupleReference*)source->data[i];

        push_vector_item(destination, copy);
    }
}

static bool tuple_references_equal(const TupleReference* left, const TupleReference* right)
{
    return left->origin == right->origin && left->reference == right->reference
        && left->offset == right->offset && left->origin_offset == right->origin_offset;
}

static void append_tuple_reference(Vector* references, TupleReference access)
{
    size_t index = 0;

    while (index < references->count && !tuple_references_equal(references->data[index], &access)) {
        index++;
    }

    if (index < references->count) {
        return;
    }

    TupleReference* copy = allocate_memory(sizeof(TupleReference));
    *copy = access;

    push_vector_item(references, copy);
}

void add_tuple_reference(Analyzer* analyzer, ASTNode* ast, TupleReference access)
{
    Vector* references = create_tuple_references(analyzer, ast);

    append_tuple_reference(references, access);
}

void merge_tuple_reference_vectors(Vector* destination, const Vector* source)
{
    for (size_t i = 0; i < source->count; i++) {
        TupleReference access = *(TupleReference*)source->data[i];

        append_tuple_reference(destination, access);
    }
}

static void retain_tuple_reference(Analyzer* analyzer, ASTNode* ast, TupleReference access, size_t start, size_t slots)
{
    if (access.offset >= start && access.offset - start < slots) {
        return;
    }

    add_tuple_reference(analyzer, ast, access);
}

void remove_tuple_references(Analyzer* analyzer, ASTNode* ast, size_t start, size_t slots)
{
    TupleReferenceState* state = find_tuple_reference_state(analyzer, ast);

    if (!state) {
        return;
    }

    Vector previous = state->references;

    init_vector(&state->references);

    for (size_t i = 0; i < previous.count; i++) {
        TupleReference access = *(TupleReference*)previous.data[i];

        retain_tuple_reference(analyzer, ast, access, start, slots);
    }

    free_tuple_reference_vector(&previous);
}

static void copy_tuple_reference(Analyzer* analyzer, ASTNode* destination, TupleReference access, size_t start,
    size_t offset, size_t slots)
{
    if (access.offset < start || access.offset - start >= slots) {
        return;
    }

    access.offset += offset - start;

    add_tuple_reference(analyzer, destination, access);
}

void copy_tuple_references(Analyzer* analyzer, ASTNode* destination, ASTNode* source, size_t start, size_t offset,
    size_t slots)
{
    const Vector* references = get_tuple_references(analyzer, source);

    for (size_t i = 0; i < references->count; i++) {
        TupleReference access = *(TupleReference*)references->data[i];

        copy_tuple_reference(analyzer, destination, access, start, offset, slots);
    }
}

void set_tuple_reference_vector(Analyzer* analyzer, ASTNode* ast, const Vector* source)
{
    if (!source->count) {
        clear_tuple_references(analyzer, ast);

        return;
    }

    Vector* destination = create_tuple_references(analyzer, ast);

    copy_tuple_reference_vector(destination, source);
}

static void free_tuple_reference_state(TupleReferenceState* state)
{
    free_tuple_reference_vector(&state->references);
    free_memory(state);
}

static void free_tuple_reference_bucket(TableItem* item)
{
    while (item) {
        free_tuple_reference_state(item->value);

        item = item->next;
    }
}

void free_tuple_reference_states(Analyzer* analyzer)
{
    Table* states = &analyzer->tuple_references;

    for (size_t i = 0; i < states->capacity; i++) {
        free_tuple_reference_bucket(states->data[i]);
    }

    free_table(states);
}

static void discard_transient_tuple_reference_state(Analyzer* analyzer, TableItem* item)
{
    TupleReferenceState* state = item->value;

    if (state->persistent) {
        return;
    }

    delete_table_at(&analyzer->tuple_references, item->key);
    free_tuple_reference_state(state);
}

static void discard_transient_tuple_reference_bucket(Analyzer* analyzer, TableItem* item)
{
    while (item) {
        TableItem* next = item->next;

        discard_transient_tuple_reference_state(analyzer, item);

        item = next;
    }
}

void discard_transient_tuple_references(Analyzer* analyzer)
{
    Table* states = &analyzer->tuple_references;

    for (size_t i = 0; i < states->capacity; i++) {
        discard_transient_tuple_reference_bucket(analyzer, states->data[i]);
    }
}
