#include "compiler/analyzer/lifetime.h"
#include "common/math.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/vector.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/reference.h"
#include "runtime/string_object.h"

typedef bool (*VisitId)(StringView id, void* context);

typedef struct ScopedUses
{
    VisitId visit;
    void* context;
    Vector shadowed;
} ScopedUses;

typedef struct ReferenceUse
{
    size_t last;
} ReferenceUse;

static bool visit_node_uses(ASTNode* ast, VisitId visit, void* context);

static bool matches_shadowed_id(StringObject* shadowed, StringView id)
{
    StringView view = get_string_view(shadowed);

    return string_views_equal(view, id);
}

static bool visit_unshadowed_use(StringView id, void* context)
{
    ScopedUses* scope = context;
    size_t index = 0;

    while (index < scope->shadowed.count && !matches_shadowed_id(scope->shadowed.data[index], id)) {
        index++;
    }

    if (index < scope->shadowed.count) {
        return false;
    }

    return scope->visit(id, scope->context);
}

static bool visit_scoped_statement_uses(ASTNode* ast, ScopedUses* scope)
{
    bool found = visit_node_uses(ast, visit_unshadowed_use, scope);

    if (ast->type == ASTVariableDefinition) {
        push_vector_item(&scope->shadowed, ast->variable_definition.id);
    }

    return found;
}

static bool visit_scoped_statement_list_uses(Vector* nodes, ScopedUses* scope)
{
    size_t count = count_vector(nodes);
    size_t index = 0;
    bool found = false;

    while (index < count && !found) {
        ASTNode* node = get_vector_at(nodes, index);

        found = visit_scoped_statement_uses(node, scope);
        index++;
    }

    return found;
}

static bool visit_scoped_block_uses(Vector* nodes, VisitId visit, void* context)
{
    ScopedUses scope = {.visit = visit, .context = context};
    bool found = visit_scoped_statement_list_uses(nodes, &scope);

    free_vector(&scope.shadowed);

    return found;
}

static bool visit_function_uses(ASTNode* ast, VisitId visit, void* context)
{
    ScopedUses scope = {.visit = visit, .context = context};
    Vector* params = &ast->function_definition.params;

    for (size_t i = 0; i < params->count; i++) {
        ASTNode* parameter = get_vector_at(params, i);

        push_vector_item(&scope.shadowed, parameter->parameter.id);
    }

    bool found = visit_node_uses(ast->function_definition.body, visit_unshadowed_use, &scope);

    free_vector(&scope.shadowed);

    return found;
}

static bool visit_node_list_uses(Vector* nodes, VisitId visit, void* context)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);
        bool found = visit_node_uses(node, visit, context);

        if (found) {
            return true;
        }
    }

    return false;
}

static bool visit_match_body_uses(MatchArm* arm, VisitId visit, void* context)
{
    if (visit_node_uses(arm->guard, visit, context)) {
        return true;
    }

    return visit_node_uses(arm->branch, visit, context);
}

static bool visit_match_arm_uses(MatchArm* arm, VisitId visit, void* context)
{
    if (visit_node_uses(arm->pattern, visit, context)) {
        return true;
    }

    if (!arm->binding) {
        return visit_match_body_uses(arm, visit, context);
    }

    void* ids[] = {arm->binding->variable_definition.id};
    ScopedUses scope = {.visit = visit, .context = context, .shadowed = {.data = ids, .count = 1}};

    return visit_match_body_uses(arm, visit_unshadowed_use, &scope);
}

static bool visit_match_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->match.subject, visit, context)) {
        return true;
    }

    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        bool found = visit_match_arm_uses(arm, visit, context);

        if (found) {
            return true;
        }
    }

    return false;
}

static bool visit_assignment_uses(ASTNode* ast, VisitId visit, void* context)
{
    Token token = ast->assignment.token;
    StringView id = create_string_view(token.chars, token.length);

    return visit(id, context) || visit_node_uses(ast->assignment.expr, visit, context);
}

static void add_for_shadowed_bindings(ASTNode* ast, ScopedUses* scope)
{
    ASTNode* index = ast->for_statement.index;
    ASTNode* value = ast->for_statement.value;

    if (index) {
        push_vector_item(&scope->shadowed, index->variable_definition.id);
    }

    push_vector_item(&scope->shadowed, value->variable_definition.id);
}

static bool visit_for_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->for_statement.array, visit, context)
        || visit_node_uses(ast->for_statement.end, visit, context)) {
        return true;
    }

    ScopedUses scope = {.visit = visit, .context = context};

    add_for_shadowed_bindings(ast, &scope);

    bool found = visit_node_uses(ast->for_statement.body, visit_unshadowed_use, &scope);

    free_vector(&scope.shadowed);

    return found;
}

static bool visit_slice_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->slice.expr, visit, context)) {
        return true;
    }

    if (visit_node_uses(ast->slice.start, visit, context)) {
        return true;
    }

    return visit_node_uses(ast->slice.end, visit, context);
}

static bool visit_index_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->index.array, visit, context)) {
        return true;
    }

    if (visit_node_uses(ast->index.index, visit, context)) {
        return true;
    }

    return visit_node_uses(ast->index.value, visit, context);
}

static bool visit_binary_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->binary.left_expr, visit, context)) {
        return true;
    }

    return visit_node_uses(ast->binary.right_expr, visit, context);
}

static bool visit_while_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->while_statement.condition, visit, context)) {
        return true;
    }

    return visit_node_uses(ast->while_statement.body, visit, context);
}

static bool visit_conditional_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (visit_node_uses(ast->conditional.condition, visit, context)) {
        return true;
    }

    if (visit_node_uses(ast->conditional.then_branch, visit, context)) {
        return true;
    }

    return visit_node_uses(ast->conditional.else_branch, visit, context);
}

static bool visit_node_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTArray:
            return visit_node_list_uses(&ast->array.elements, visit, context);
        case ASTTuple:
            return visit_node_list_uses(&ast->tuple.elements, visit, context);
        case ASTSlice:
            return visit_slice_uses(ast, visit, context);
        case ASTIndex:
        case ASTIndexAssignment:
            return visit_index_uses(ast, visit, context);
        case ASTAssignment:
            return visit_assignment_uses(ast, visit, context);
        case ASTBinary:
            return visit_binary_uses(ast, visit, context);
        case ASTBuiltinCall:
            return visit_node_list_uses(&ast->builtin_call.args, visit, context);
        case ASTCompound:
            return visit_scoped_block_uses(&ast->compound.statements, visit, context);
        case ASTFor:
            return visit_for_uses(ast, visit, context);
        case ASTWhile:
            return visit_while_uses(ast, visit, context);
        case ASTConditional:
            return visit_conditional_uses(ast, visit, context);
        case ASTConversion:
            return visit_node_uses(ast->conversion.expr, visit, context);
        case ASTFunctionCall:
            return visit_node_list_uses(&ast->function_call.args, visit, context);
        case ASTTestBlock:
            return visit_node_uses(ast->test_block.function, visit, context);
        case ASTFunctionDefinition:
            return visit_function_uses(ast, visit, context);
        case ASTMatch:
            return visit_match_uses(ast, visit, context);
        case ASTPrefix:
            return visit_node_uses(ast->prefix.expr, visit, context);
        case ASTReturn:
            return visit_node_uses(ast->return_statement.expr, visit, context);
        case ASTVariable: {
            StringView id = get_string_view(ast->variable.id);

            return visit(id, context);
        }
        case ASTVariableDefinition:
            return visit_node_uses(ast->variable_definition.expr, visit, context);
        default:
            return false;
    }
}

static bool matches_reference_id(StringView id, void* context)
{
    return string_views_equal(id, *(StringView*)context);
}

static bool record_reference_use(StringView id, void* context)
{
    ReferenceLifetimes* lifetimes = context;
    ReferenceUse* use = get_table_at(&lifetimes->last_uses, id);

    if (use) {
        use->last = lifetimes->position;

        return false;
    }

    use = allocate_memory(sizeof(ReferenceUse));
    use->last = lifetimes->position;
    set_table_at(&lifetimes->last_uses, id, use);

    return false;
}

static void record_pending_reference_uses(ReferenceLifetimes* lifetimes, Vector* nodes, size_t start)
{
    size_t count = count_vector(nodes);

    for (size_t i = start; i < count; i++) {
        ASTNode* ast = get_vector_at(nodes, i);
        lifetimes->position = i;

        visit_node_uses(ast, record_reference_use, lifetimes);
    }
}

static void schedule_previous_reference_releases(ReferenceLifetimes* lifetimes, Vector* nodes, size_t start)
{
    for (size_t i = 0; i < start; i++) {
        ASTNode* ast = get_vector_at(nodes, i);

        schedule_reference_release(lifetimes, ast, start);
    }
}

void init_reference_lifetimes(ReferenceLifetimes* lifetimes, Vector* nodes, size_t start)
{
    size_t count = count_vector(nodes);

    init_table(&lifetimes->last_uses, count * 2);

    lifetimes->releases = allocate_zeroed_memory(count, sizeof(Vector));

    record_pending_reference_uses(lifetimes, nodes, start);
    schedule_previous_reference_releases(lifetimes, nodes, start);
}

void schedule_reference_release(ReferenceLifetimes* lifetimes, ASTNode* ast, size_t position)
{
    if (!is_variable_definition(ast) || !ast->variable_definition.reference_access_active) {
        return;
    }

    StringView id = get_string_view(ast->variable_definition.id);
    ReferenceUse* use = get_table_at(&lifetimes->last_uses, id);

    if (use) {
        position = MAX(position, use->last);
    }

    push_vector_item(&lifetimes->releases[position], ast);
}

void free_reference_lifetimes(ReferenceLifetimes* lifetimes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free_vector(&lifetimes->releases[i]);
    }

    for (size_t i = 0; i < lifetimes->last_uses.capacity; i++) {
        TableItem* item = lifetimes->last_uses.data[i];

        while (item) {
            free_memory(item->value);
            item = item->next;
        }
    }

    free_memory(lifetimes->releases);
    free_table(&lifetimes->last_uses);
}

bool branch_uses_reference(ASTNode* branch, StringObject* id, ASTNode* binding)
{
    if (!branch) {
        return false;
    }

    StringView view = get_string_view(id);

    if (binding && matches_shadowed_id(binding->variable_definition.id, view)) {
        return false;
    }

    return visit_node_uses(branch, matches_reference_id, &view);
}

bool continuation_uses_reference(Analyzer* analyzer, StringObject* id)
{
    LoopPath* loop = analyzer->loop_path;

    while (loop) {
        if (branch_uses_reference(loop->ast, id, NULL)) {
            return true;
        }

        loop = loop->previous;
    }

    ReferenceLifetimes* lifetimes = analyzer->reference_lifetimes;

    if (!lifetimes) {
        return false;
    }

    StringView view = get_string_view(id);
    ReferenceUse* use = get_table_at(&lifetimes->last_uses, view);

    return use && use->last >= analyzer->next_node;
}
