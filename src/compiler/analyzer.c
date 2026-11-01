#include "compiler/analyzer.h"
#include "builtin/builtin.h"
#include "builtin/io.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "compiler/validation.h"
#include "runtime/string_object.h"
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef bool (*VisitId)(StringView id, void* context);

static bool visit_node_uses(ASTNode* ast, VisitId visit, void* context);
static void clear_node_analysis(ASTNode* ast, Vector* functions);
static void analyze_node(Analyzer* analyzer, ASTNode* ast);
static void validate_node_global_access(Analyzer* analyzer, ASTNode* ast);
static void activate_reference_access(ASTNode* ast);
static void transfer_exclusive_reference(ASTNode* expression);
static void update_boolean_match_coverage(MatchArm* arm, bool* covers_false, bool* covers_true);
static ASTNode* find_node_return(ASTNode* ast);
static void apply_declared_type(ASTNode* expression, DataType type, ReferenceType reference_type);

typedef struct FunctionPath
{
    ASTNode* function;
    struct FunctionPath* previous;
} FunctionPath;

typedef struct LoopPath
{
    ASTNode* ast;
    struct LoopPath* previous;
} LoopPath;

typedef struct OwnershipState
{
    ASTNode* symbol;
    Vector reference_origins;
    size_t shared_access_count;
    bool exclusive_access_active;
    bool initialized;
    bool moved;
    bool reference_access_active;
} OwnershipState;

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

typedef struct ReferenceLifetimes
{
    size_t position;
    Table last_uses;
    Vector* releases;
} ReferenceLifetimes;

static _Noreturn void binary_type_error(Token token, DataType left, DataType right)
{
    const char* left_name = get_data_type_name(left);
    const char* right_name = get_data_type_name(right);

    fatal_source_error(token.line, token.column, "No lossless common numeric type for %s and %s", left_name, right_name);
}

static _Noreturn void binding_access_error(char* message, Token token, char* detail)
{
    fatal_token_error(token, detail, "%s", message);
}

static _Noreturn void float_literal_out_of_range_error(Token token, DataType type)
{
    const char* type_name = get_data_type_name(type);

    fatal_source_error(token.line, token.column, "Float literal is outside the range of %s", type_name);
}

static _Noreturn void float_literal_requires_type_error(Token token)
{
    fatal_source_error(token.line, token.column, "Float literal requires an explicit type");
}

static _Noreturn void implicit_conversion_error(Token token, DataType source, DataType destination)
{
    const char* source_name = get_data_type_name(source);
    const char* destination_name = get_data_type_name(destination);

    fatal_token_error(token, "", "Cannot implicitly convert %s to %s for binding ", source_name, destination_name);
}

static _Noreturn void integer_literal_not_representable_error(Token token, DataType type)
{
    const char* type_name = get_data_type_name(type);

    fatal_source_error(token.line, token.column, "Integer literal cannot be represented as %s", type_name);
}

static _Noreturn void integer_literal_out_of_range_error(Token token, DataType type)
{
    const char* type_name = get_data_type_name(type);

    fatal_source_error(token.line, token.column, "Integer literal is outside the range of %s", type_name);
}

static _Noreturn void integer_literal_requires_type_error(Token token)
{
    fatal_source_error(token.line, token.column, "Integer literal requires an explicit type");
}

static _Noreturn void invalid_builtin_arguments_error(Token token)
{
    fatal_token_error(token, "", "Invalid arguments to builtin ");
}

static _Noreturn void negative_unsigned_integer_error(Token token)
{
    fatal_source_error(token.line, token.column, "Negative integer literal cannot have an unsigned type");
}

static _Noreturn void redefinition_error(char* kind, Token token)
{
    fatal_token_error(token, " is already defined", "%s ", kind);
}

static _Noreturn void return_outside_function_error(Token token)
{
    fatal_source_error(token.line, token.column, "Return statement is only valid inside a function");
}

static _Noreturn void semantic_error(char* message, Token token)
{
    fatal_source_error(token.line, token.column, "%s", message);
}

static _Noreturn void semantic_token_error(char* message, Token token)
{
    fatal_token_error(token, "", "%s", message);
}

static _Noreturn void symbol_error(const char* detail, Token token)
{
    fatal_token_error(token, detail, "");
}

static _Noreturn void unsupported_explicit_conversion_error(Token token, DataType source, DataType destination)
{
    const char* source_name = get_data_type_name(source);
    const char* destination_name = get_data_type_name(destination);

    fatal_source_error(token.line, token.column, "Unsupported explicit conversion from %s to %s",
        source_name, destination_name);
}

static ASTNode* get_integer_literal(ASTNode* expression, bool* negative)
{
    *negative = false;

    if (expression->type == ASTInteger && is_signed_integer_type(expression->integer_literal.type_id)
        && expression->integer_literal.value > INT64_MAX) {
        return NULL;
    }

    if (expression->type == ASTInteger) {
        return expression;
    }

    if (expression->type != ASTPrefix || expression->prefix.operator.type != TokenMinus
        || expression->prefix.expr->type != ASTInteger) {
        return NULL;
    }

    *negative = true;

    return expression->prefix.expr;
}

static bool integer_literal_fits_type(uint64_t value, bool negative, DataType type)
{
    size_t bits = get_integer_type_size(type) * CHAR_BIT;

    if (!bits || (!is_signed_integer_type(type) && negative)) {
        return false;
    }

    if (!is_signed_integer_type(type)) {
        return bits == 64 || value <= (UINT64_C(1) << bits) - 1;
    }

    uint64_t limit = UINT64_C(1) << (bits - 1);

    return negative ? value <= limit : value < limit;
}

static bool integer_literal_fits_float(uint64_t value, DataType type)
{
    size_t precision;

    if (type == TypeF32) {
        precision = FLT_MANT_DIG;
    } else if (type == TypeF64) {
        precision = DBL_MANT_DIG;
    } else {
        return false;
    }

    size_t bits = 0;
    uint64_t remaining = value;

    while (remaining) {
        bits++;
        remaining >>= 1;
    }

    if (bits <= precision) {
        return true;
    }

    size_t discarded_bits = bits - precision;
    uint64_t mask = (UINT64_C(1) << discarded_bits) - 1;

    return (value & mask) == 0;
}

static bool apply_integer_literal_type(ASTNode* expression, DataType type)
{
    bool negative;
    ASTNode* literal = get_integer_literal(expression, &negative);

    if (!literal) {
        return false;
    }

    if (negative && is_integer_type(type) && !is_signed_integer_type(type)) {
        negative_unsigned_integer_error(literal->integer_literal.token);
    }

    bool fits_integer = integer_literal_fits_type(literal->integer_literal.value, negative, type);
    bool fits_float = integer_literal_fits_float(literal->integer_literal.value, type);

    if (is_float_type(type) && !fits_float) {
        integer_literal_not_representable_error(literal->integer_literal.token, type);
    }

    if (is_integer_type(type) && !fits_integer) {
        integer_literal_out_of_range_error(literal->integer_literal.token, type);
    }

    literal->integer_literal.type_id = type;

    return true;
}

static ASTNode* get_float_literal(ASTNode* expression)
{
    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenMinus) {
        expression = expression->prefix.expr;
    }

    if (expression->type != ASTFloat) {
        return NULL;
    }

    return expression;
}

static void apply_literal_type(ASTNode* expression, DataType type)
{
    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenKeywordConst) {
        apply_literal_type(expression->prefix.expr, type);

        return;
    }

    if (is_integer_type(type) || is_float_type(type)) {
        apply_integer_literal_type(expression, type);
    }

    expression = get_float_literal(expression);

    if (!expression || !is_float_type(type)) {
        return;
    }

    if (type == TypeF32 && expression->float_literal.value > FLT_MAX) {
        float_literal_out_of_range_error(expression->float_literal.token, type);
    }

    expression->float_literal.type_id = type;
}

static ASTNode* find_frame_symbol(Scope* scope, StringView id)
{
    Scope* frame = scope->frame_scope;
    ASTNode* symbol = NULL;

    for (Scope* current = scope;
        current && current->frame_scope == frame && !symbol;
        current = current->parent) {
        symbol = get_local_symbol(current, id);
    }

    return symbol;
}

static ASTNode* find_symbol(Analyzer* analyzer, StringView id)
{
    ASTNode* symbol = find_frame_symbol(analyzer->current_scope, id);
    if (symbol) {
        return symbol;
    }

    return get_local_symbol(analyzer->top_level->compound.scope, id);
}

static size_t* get_shared_access_count(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return &symbol->parameter.shared_access_count;
    }

    return &symbol->variable_definition.shared_access_count;
}

static bool has_exclusive_access(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.exclusive_access_active;
    }

    return symbol->variable_definition.exclusive_access_active;
}

static void set_exclusive_access(ASTNode* symbol, bool active)
{
    if (is_parameter(symbol)) {
        symbol->parameter.exclusive_access_active = active;

        return;
    }

    symbol->variable_definition.exclusive_access_active = active;
}

static bool is_moved(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.moved;
    }

    return symbol->variable_definition.moved;
}

static void set_moved(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        symbol->parameter.moved = true;

        return;
    }

    symbol->variable_definition.moved = true;
}

static void set_moved_state(ASTNode* symbol, bool moved)
{
    if (is_parameter(symbol)) {
        symbol->parameter.moved = moved;

        return;
    }

    symbol->variable_definition.moved = moved;
}

static ASTNode* get_reference_origin_or_self(ASTNode* symbol)
{
    ASTNode* origin = get_reference_origin(symbol);

    if (origin) {
        return origin;
    }

    return symbol;
}

static ASTNode* reference_origin_from_expression(ASTNode* ast)
{
    if (ast->type == ASTPrefix && is_variable(ast->prefix.expr)) {
        ASTNode* symbol = ast->prefix.expr->variable.symbol;

        return get_reference_origin_or_self(symbol);
    }

    return get_reference_origin(ast);
}

static Vector* reference_origins_from_expression(ASTNode* ast)
{
    Vector* origins = get_reference_origins(ast);

    if (origins) {
        return origins;
    }

    static Vector empty;

    return &empty;
}

static bool contains_reference_origin(Vector* origins, ASTNode* origin)
{
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        if (get_vector_at(origins, i) == origin) {
            return true;
        }
    }

    return false;
}

static void add_reference_origin(Vector* origins, ASTNode* origin)
{
    if (!origin || contains_reference_origin(origins, origin)) {
        return;
    }

    push_vector_item(origins, origin);
}

static void copy_reference_origins(Vector* destination, Vector* source)
{
    if (destination == source) {
        return;
    }

    destination->count = 0;
    size_t count = count_vector(source);

    for (size_t i = 0; i < count; i++) {
        add_reference_origin(destination, get_vector_at(source, i));
    }
}

static void merge_reference_origins(Vector* destination, Vector* source)
{
    size_t count = count_vector(source);

    for (size_t i = 0; i < count; i++) {
        add_reference_origin(destination, get_vector_at(source, i));
    }
}

static void set_reference_origins_from_expression(ASTNode* destination, ASTNode* expression)
{
    Vector* destination_origins = get_reference_origins(destination);
    Vector* origins = reference_origins_from_expression(expression);
    copy_reference_origins(destination_origins, origins);
}

static bool matches_shadowed_id(StringObject* shadowed, StringView id)
{
    StringView view = get_string_view(shadowed);

    return compare_string_view(view, id);
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

static bool visit_match_arm_uses(MatchArm* arm, VisitId visit, void* context)
{
    if (visit_node_uses(arm->pattern, visit, context)) {
        return true;
    }

    if (!arm->binding) {
        return visit_node_uses(arm->guard, visit, context) || visit_node_uses(arm->branch, visit, context);
    }

    void* ids[] = {arm->binding->variable_definition.id};
    ScopedUses scope = {.visit = visit, .context = context, .shadowed = {.data = ids, .count = 1}};

    return visit_node_uses(arm->guard, visit_unshadowed_use, &scope)
        || visit_node_uses(arm->branch, visit_unshadowed_use, &scope);
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

    return visit_node_uses(ast->match.default_branch, visit, context);
}

static bool visit_assignment_uses(ASTNode* ast, VisitId visit, void* context)
{
    Token token = ast->assignment.token;
    StringView id = create_string_view(token.chars, token.length);

    return visit(id, context) || visit_node_uses(ast->assignment.expr, visit, context);
}

static bool visit_node_uses(ASTNode* ast, VisitId visit, void* context)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTAssignment:
            return visit_assignment_uses(ast, visit, context);
        case ASTBinary:
            return visit_node_uses(ast->binary.left_expr, visit, context)
                || visit_node_uses(ast->binary.right_expr, visit, context);
        case ASTBuiltinCall:
            return visit_node_list_uses(&ast->builtin_call.args, visit, context);
        case ASTCompound:
            return visit_scoped_block_uses(&ast->compound.statements, visit, context);
        case ASTWhile:
            return visit_node_uses(ast->while_statement.condition, visit, context)
                || visit_node_uses(ast->while_statement.body, visit, context);
        case ASTConditional:
            return visit_node_uses(ast->conditional.condition, visit, context)
                || visit_node_uses(ast->conditional.then_branch, visit, context)
                || visit_node_uses(ast->conditional.else_branch, visit, context);
        case ASTConversion:
            return visit_node_uses(ast->conversion.expr, visit, context);
        case ASTFunctionCall:
            return visit_node_list_uses(&ast->function_call.args, visit, context);
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
    return compare_string_view(id, *(StringView*)context);
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

static void init_reference_lifetimes(ReferenceLifetimes* lifetimes, Vector* nodes, size_t start)
{
    size_t count = count_vector(nodes);

    init_table(&lifetimes->last_uses, count * 2);

    lifetimes->releases = allocate_zeroed_memory(count, sizeof(Vector));

    for (size_t i = start; i < count; i++) {
        ASTNode* ast = get_vector_at(nodes, i);
        lifetimes->position = i;

        visit_node_uses(ast, record_reference_use, lifetimes);
    }
}

static void schedule_reference_release(ReferenceLifetimes* lifetimes, ASTNode* ast, size_t position)
{
    if (!is_variable_definition(ast) || get_reference_type(ast) == RefNone) {
        return;
    }

    StringView id = get_string_view(ast->variable_definition.id);
    ReferenceUse* use = get_table_at(&lifetimes->last_uses, id);

    if (use) {
        position = MAX(position, use->last);
    }

    push_vector_item(&lifetimes->releases[position], ast);
}

static void free_reference_lifetimes(ReferenceLifetimes* lifetimes, size_t count)
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

static void release_reference_access(ASTNode* reference)
{
    Vector* origins = get_reference_origins(reference);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);

        if (reference->variable_definition.reference_type == RefShared) {
            (*get_shared_access_count(origin))--;
        } else {
            set_exclusive_access(origin, false);
        }
    }

    reference->variable_definition.reference_access_active = false;
}

static void release_inactive_reference_access(Vector* references)
{
    size_t count = count_vector(references);

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = get_vector_at(references, i);

        if (ast->variable_definition.reference_access_active) {
            release_reference_access(ast);
        }
    }
}

static void analyze_nodes(Analyzer* analyzer, Vector* nodes, size_t start)
{
    size_t count = count_vector(nodes);

    if (start >= count) {
        return;
    }

    ReferenceLifetimes lifetimes;
    ReferenceLifetimes* previous_lifetimes = analyzer->reference_lifetimes;
    Vector* previous_nodes = analyzer->current_nodes;
    size_t previous_next = analyzer->next_node;

    init_reference_lifetimes(&lifetimes, nodes, start);

    analyzer->reference_lifetimes = &lifetimes;

    for (size_t i = 0; i < start; i++) {
        ASTNode* ast = get_vector_at(nodes, i);

        schedule_reference_release(&lifetimes, ast, start);
    }

    for (size_t i = start; i < count; i++) {
        ASTNode* ast = get_vector_at(nodes, i);
        analyzer->current_nodes = nodes;
        analyzer->next_node = i + 1;

        analyze_node(analyzer, ast);
        schedule_reference_release(&lifetimes, ast, i);
        release_inactive_reference_access(&lifetimes.releases[i]);
    }

    analyzer->reference_lifetimes = previous_lifetimes;
    analyzer->current_nodes = previous_nodes;
    analyzer->next_node = previous_next;

    free_reference_lifetimes(&lifetimes, count);
}

static void analyze_expression_nodes(Analyzer* analyzer, Vector* nodes)
{
    analyze_nodes(analyzer, nodes, 0);
}

static ASTNode* get_branch_result(ASTNode* branch)
{
    if (branch->type == ASTConditional) {
        return branch;
    }

    Vector* statements = &branch->compound.statements;
    size_t count = count_vector(statements);

    if (!count) {
        return NULL;
    }

    return get_vector_at(statements, count - 1);
}

static void set_expression_reference_type(ASTNode* ast, ReferenceType type)
{
    if (ast->type == ASTConditional) {
        ast->conditional.reference_type = type;

        return;
    }

    ast->match.reference_type = type;
}

static void merge_branch_reference(
    ASTNode* ast, ASTNode* result, bool first, bool has_expected_type, Token token)
{
    ReferenceType type = get_reference_type(result);

    if (first && !has_expected_type) {
        set_expression_reference_type(ast, type);
    } else if (get_reference_type(ast) != type) {
        semantic_token_error("Expression branches have incompatible reference types ", token);
    }

    Vector* origins = reference_origins_from_expression(result);
    Vector* expression_origins = get_reference_origins(ast);
    merge_reference_origins(expression_origins, origins);
}

static Scope* enter_branch_scope(Analyzer* analyzer, ASTNode* branch, ASTNode* binding)
{
    Scope* previous_scope = analyzer->current_scope;
    branch->compound.scope = create_scope(previous_scope);
    analyzer->current_scope = branch->compound.scope;

    if (binding) {
        binding->variable_definition.scope = analyzer->current_scope;
        binding->variable_definition.position = get_next_local_position(analyzer->current_scope);
        
        StringView id = get_string_view(binding->variable_definition.id);

        set_local_variable_symbol(analyzer->current_scope, id, binding);
    }

    return previous_scope;
}

static DataType analyze_branch_contents(Analyzer* analyzer, ASTNode* branch, DataType type)
{
    size_t count = count_vector(&branch->compound.statements);

    if (count && is_integer_type(type)) {
        ASTNode* result = get_vector_at(&branch->compound.statements, count - 1);
        apply_integer_literal_type(result, type);
    }

    analyze_expression_nodes(analyzer, &branch->compound.statements);

    if (!count) {
        return TypeVoid;
    }

    ASTNode* result = get_vector_at(&branch->compound.statements, count - 1);
    DataType result_type = get_node_data_type(result);

    if (result_type == type || can_implicitly_widen_type(result_type, type)) {
        return type;
    }

    return result_type;
}

static DataType analyze_scoped_branch(Analyzer* analyzer, ASTNode* branch, ASTNode* binding, DataType type)
{
    Scope* previous_scope = enter_branch_scope(analyzer, branch, binding);
    DataType result_type = analyze_branch_contents(analyzer, branch, type);
    analyzer->current_scope = previous_scope;

    return result_type;
}

static DataType analyze_else_branch(Analyzer* analyzer, ASTNode* branch, DataType type)
{
    if (!branch) {
        return TypeVoid;
    }

    if (branch->type == ASTConditional) {
        branch->conditional.type_id = type;
        analyze_node(analyzer, branch);

        return get_node_data_type(branch);
    }

    return analyze_scoped_branch(analyzer, branch, NULL, type);
}

static void capture_symbol_ownership(Vector* states, ASTNode* symbol)
{
    if (!is_variable_type(symbol)) {
        return;
    }

    OwnershipState* state = allocate_memory(sizeof(OwnershipState));
    state->symbol = symbol;
    init_vector(&state->reference_origins);
    Vector* origins = get_reference_origins(symbol);
    copy_reference_origins(&state->reference_origins, origins);
    state->shared_access_count = *get_shared_access_count(symbol);
    state->exclusive_access_active = has_exclusive_access(symbol);
    state->initialized = is_initialized(symbol);
    state->moved = is_moved(symbol);
    state->reference_access_active = is_variable_definition(symbol) && symbol->variable_definition.reference_access_active;

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

static void capture_ownership_state(Vector* states, Scope* scope)
{
    init_vector(states);

    while (scope) {
        capture_scope_ownership(states, scope);
        scope = scope->parent;
    }
}

static void copy_ownership_state(Vector* destination, Vector* source)
{
    init_vector(destination);

    for (size_t i = 0; i < count_vector(source); i++) {
        OwnershipState* source_state = get_vector_at(source, i);
        OwnershipState* destination_state = allocate_memory(sizeof(OwnershipState));

        *destination_state = *source_state;
        init_vector(&destination_state->reference_origins);
        copy_reference_origins(&destination_state->reference_origins, &source_state->reference_origins);
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
    symbol->variable_definition.reference_access_active = state->reference_access_active;
}

static void apply_ownership_states(Vector* states)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        apply_ownership_state(get_vector_at(states, i));
    }
}

static void update_ownership_states(Vector* states)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        OwnershipState* state = get_vector_at(states, i);
        ASTNode* symbol = state->symbol;

        Vector* origins = get_reference_origins(symbol);
        copy_reference_origins(&state->reference_origins, origins);
        state->shared_access_count = *get_shared_access_count(symbol);
        state->exclusive_access_active = has_exclusive_access(symbol);
        state->initialized = is_initialized(symbol);
        state->moved = is_moved(symbol);
        state->reference_access_active = is_variable_definition(symbol)
            && symbol->variable_definition.reference_access_active;
    }
}

static void free_ownership_states(Vector* states)
{
    for (size_t i = 0; i < count_vector(states); i++) {
        OwnershipState* state = get_vector_at(states, i);
        free_vector(&state->reference_origins);
        free_memory(state);
    }

    free_vector(states);
}

static bool branch_uses_reference(ASTNode* branch, StringObject* id, ASTNode* binding)
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

static bool continuation_uses_reference(Analyzer* analyzer, StringObject* id)
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

static void release_references_dead_on_branch(Analyzer* analyzer, Vector* states, ASTNode* branch, ASTNode* binding)
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

static void analyze_ownership_branch(
    Analyzer* analyzer,
    Vector* states,
    ASTNode* branch,
    DataType expected_type,
    DataType* type)
{
    apply_ownership_states(states);
    release_references_dead_on_branch(analyzer, states, branch, NULL);
    *type = analyze_else_branch(analyzer, branch, expected_type);
    update_ownership_states(states);
}

static void merge_ownership_state(OwnershipState* then_state, OwnershipState* else_state)
{
    OwnershipState merged = *then_state;
    init_vector(&merged.reference_origins);
    copy_reference_origins(&merged.reference_origins, &then_state->reference_origins);
    merge_reference_origins(&merged.reference_origins, &else_state->reference_origins);
    merged.shared_access_count = MAX(then_state->shared_access_count, else_state->shared_access_count);
    merged.exclusive_access_active = then_state->exclusive_access_active || else_state->exclusive_access_active;
    merged.initialized = then_state->initialized && else_state->initialized;
    merged.moved = then_state->moved || else_state->moved;
    merged.reference_access_active = then_state->reference_access_active || else_state->reference_access_active;

    apply_ownership_state(&merged);
    free_vector(&merged.reference_origins);
}

static void merge_ownership_states(Vector* then_states, Vector* else_states)
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

static void validate_loop_ownership(Vector* entry_states, Token token)
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
    }
}

static void analyze_while(Analyzer* analyzer, ASTNode* ast)
{
    Vector entry_states;
    Vector body_states;
    LoopPath path = {ast, analyzer->loop_path};
    analyzer->loop_path = &path;

    capture_ownership_state(&entry_states, analyzer->current_scope);
    copy_ownership_state(&body_states, &entry_states);
    analyze_node(analyzer, ast->while_statement.condition);

    DataType type = get_node_data_type(ast->while_statement.condition);

    if (type != TypeBool) {
        semantic_error("While condition must have type bool", ast->while_statement.token);
    }

    analyze_scoped_branch(analyzer, ast->while_statement.body, NULL, TypeVoid);
    validate_loop_ownership(&entry_states, ast->while_statement.token);
    update_ownership_states(&body_states);

    analyzer->loop_path = path.previous;

    merge_ownership_states(&entry_states, &body_states);
    free_ownership_states(&entry_states);
    free_ownership_states(&body_states);
}

static void validate_expression_branch(ASTNode* branch, Token token)
{
    if (branch->type == ASTConditional) {
        return;
    }

    Vector* statements = &branch->compound.statements;
    if (count_vector(statements) != 1) {
        semantic_token_error("Expression branch must contain exactly one expression ", token);
    }

    ASTNode* expression = get_vector_at(statements, 0);
    if (!is_expression_statement(expression)) {
        semantic_token_error("Expression branch must contain exactly one expression ", token);
    }
}

static void validate_conditional_expression(ASTNode* ast)
{
    if (!ast->conditional.expression) {
        return;
    }

    validate_expression_branch(ast->conditional.then_branch, ast->conditional.token);

    if (ast->conditional.else_branch) {
        validate_expression_branch(ast->conditional.else_branch, ast->conditional.token);
    }
}

static void merge_ownership_outcome(Vector* merged_states, Vector* states, bool* initialized)
{
    if (!*initialized) {
        copy_ownership_state(merged_states, states);
        *initialized = true;

        return;
    }

    merge_ownership_states(merged_states, states);
    update_ownership_states(merged_states);
}

static void analyze_conditional(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuation_nodes = analyzer->current_nodes;
    size_t continuation_start = analyzer->next_node;

    validate_conditional_expression(ast);
    analyze_node(analyzer, ast->conditional.condition);

    if (get_node_data_type(ast->conditional.condition) != TypeBool) {
        semantic_error("Condition must have type bool", ast->conditional.token);
    }

    Vector then_states;
    Vector else_states;

    capture_ownership_state(&then_states, analyzer->current_scope);
    copy_ownership_state(&else_states, &then_states);

    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;
    release_references_dead_on_branch(analyzer, &then_states, ast->conditional.then_branch, NULL);

    DataType type = ast->conditional.type_id;
    DataType then_type = analyze_scoped_branch(analyzer, ast->conditional.then_branch, NULL, type);

    update_ownership_states(&then_states);

    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;

    DataType else_type;

    analyze_ownership_branch(analyzer, &else_states, ast->conditional.else_branch, type, &else_type);
    merge_ownership_states(&then_states, &else_states);
    free_ownership_states(&then_states);
    free_ownership_states(&else_states);

    if (!ast->conditional.expression) {
        ast->conditional.type_id = TypeVoid;

        return;
    }

    if (!ast->conditional.else_branch) {
        semantic_token_error("If expression requires an else branch ", ast->conditional.token);
    }

    if (then_type == TypeVoid || else_type == TypeVoid) {
        semantic_token_error("Conditional expression branch must produce a value ", ast->conditional.token);
    }

    if (then_type != else_type) {
        semantic_token_error("Conditional expression branches have incompatible types ", ast->conditional.token);
    }

    bool has_expected_type = ast->conditional.type_id != TypeUnknown
        && ast->conditional.type_id != TypeVoid;
    ASTNode* then_result = get_branch_result(ast->conditional.then_branch);
    ASTNode* else_result = get_branch_result(ast->conditional.else_branch);
    merge_branch_reference(ast, then_result, true, has_expected_type, ast->conditional.token);
    merge_branch_reference(ast, else_result, false, has_expected_type, ast->conditional.token);

    ast->conditional.type_id = then_type;
}

static bool match_has_wildcard(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        if (!arm->pattern && !arm->guard) {
            return true;
        }
    }

    return false;
}

static bool update_alternative_boolean_coverage(ASTNode* pattern, bool* covers_false, bool* covers_true)
{
    if (pattern->type != ASTBinary || pattern->binary.operator.type != TokenPipe) {
        return false;
    }

    MatchArm left_arm = { .pattern = pattern->binary.left_expr };
    MatchArm right_arm = { .pattern = pattern->binary.right_expr };
    
    update_boolean_match_coverage(&left_arm, covers_false, covers_true);
    update_boolean_match_coverage(&right_arm, covers_false, covers_true);

    return true;
}

static void update_boolean_match_coverage(MatchArm* arm, bool* covers_false, bool* covers_true)
{
    if (arm->guard) {
        return;
    }

    ASTNode* pattern = arm->pattern;
    if (!pattern) {
        return;
    }

    if (update_alternative_boolean_coverage(pattern, covers_false, covers_true)) {
        return;
    }

    if (pattern->type != ASTBoolean) {
        return;
    }

    if (pattern->boolean_literal.value) {
        *covers_true = true;

        return;
    }

    *covers_false = true;
}

static bool match_has_boolean_subject(ASTNode* ast)
{
    if (!ast->match.subject) {
        return true;
    }

    return get_node_data_type(ast->match.subject) == TypeBool;
}

static bool match_covers_both_booleans(ASTNode* ast)
{
    if (!match_has_boolean_subject(ast)) {
        return false;
    }

    bool covers_false = false;
    bool covers_true = false;
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        update_boolean_match_coverage(arm, &covers_false, &covers_true);
    }

    return covers_false && covers_true;
}

static bool match_is_exhaustive(ASTNode* ast)
{
    return ast->match.default_branch || match_has_wildcard(ast) || match_covers_both_booleans(ast);
}

static void merge_match_branch_type(ASTNode* ast, DataType branch_type, DataType* result_type)
{
    if (branch_type == TypeVoid) {
        semantic_token_error("Match expression arm must produce a value ", ast->match.token);
    }

    if (*result_type == TypeUnknown) {
        *result_type = branch_type;

        return;
    }

    if (*result_type != branch_type) {
        semantic_token_error("Match expression arms have incompatible types ", ast->match.token);
    }
}

static void analyze_match_pattern_node(Analyzer* analyzer, ASTNode* ast, ASTNode* pattern)
{
    if (pattern->type == ASTBinary && pattern->binary.operator.type == TokenPipe) {
        analyze_match_pattern_node(analyzer, ast, pattern->binary.left_expr);
        analyze_match_pattern_node(analyzer, ast, pattern->binary.right_expr);

        return;
    }

    analyze_node(analyzer, pattern);
    DataType expected = ast->match.subject ? get_node_data_type(ast->match.subject) : TypeBool;

    if (is_integer_type(expected)) {
        apply_integer_literal_type(pattern, expected);
    }

    if (expected == TypeString) {
        semantic_token_error("String match patterns are not supported ", ast->match.token);
    }

    if (get_node_data_type(pattern) != expected) {
        semantic_token_error("Match pattern type does not match subject ", ast->match.token);
    }
}

static void analyze_match_pattern(Analyzer* analyzer, ASTNode* ast, MatchArm* arm)
{
    if (!arm->pattern) {
        return;
    }

    analyze_match_pattern_node(analyzer, ast, arm->pattern);
}

static void restore_match_continuation(Analyzer* analyzer, Vector* continuation_nodes, size_t continuation_start)
{
    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;
}

static void merge_match_result(ASTNode* ast, ASTNode* branch, DataType branch_type, DataType* result_type)
{
    if (!ast->match.expression) {
        return;
    }

    bool first = *result_type == TypeUnknown;
    bool has_expected_type = ast->match.type_id != TypeUnknown && ast->match.type_id != TypeVoid;
    ASTNode* result = get_branch_result(branch);
    
    merge_branch_reference(ast, result, first, has_expected_type, ast->match.token);
    merge_match_branch_type(ast, branch_type, result_type);
}

static void analyze_match_guard(Analyzer* analyzer, MatchArm* arm, Vector* fallthrough_states)
{
    if (!arm->guard) {
        return;
    }

    analyze_node(analyzer, arm->guard);

    if (get_node_data_type(arm->guard) != TypeBool) {
        semantic_error("Match guard requires a bool operand", arm->guard_token);
    }

    Vector guard_states;

    copy_ownership_state(&guard_states, fallthrough_states);
    update_ownership_states(&guard_states);

    if (arm->pattern) {
        merge_ownership_states(fallthrough_states, &guard_states);
        update_ownership_states(fallthrough_states);
        apply_ownership_states(&guard_states);
    } else {
        update_ownership_states(fallthrough_states);
    }

    release_references_dead_on_branch(analyzer, &guard_states, arm->branch, arm->binding);
    free_ownership_states(&guard_states);
}

static void analyze_match_arm(Analyzer* analyzer, ASTNode* ast, MatchArm* arm, Vector* continuation_nodes,
    size_t continuation_start, DataType* result_type, Vector* fallthrough_states)
{
    restore_match_continuation(analyzer, continuation_nodes, continuation_start);
    analyze_match_pattern(analyzer, ast, arm);

    if (arm->binding) {
        arm->binding->variable_definition.type_id = get_node_data_type(ast->match.subject);
    }

    Scope* previous_scope = enter_branch_scope(analyzer, arm->branch, arm->binding);

    analyze_match_guard(analyzer, arm, fallthrough_states);

    DataType branch_type = analyze_branch_contents(analyzer, arm->branch, ast->match.type_id);
    analyzer->current_scope = previous_scope;

    merge_match_result(ast, arm->branch, branch_type, result_type);
}

static void analyze_match_arm_ownership(Analyzer* analyzer, ASTNode* ast, MatchArm* arm,
    Vector* continuation_nodes, size_t continuation_start, DataType* result_type,
    Vector* initial_states, Vector* merged_states, bool* merged_initialized)
{
    Vector states;
    copy_ownership_state(&states, initial_states);
    apply_ownership_states(&states);

    if (!arm->guard) {
        release_references_dead_on_branch(analyzer, &states, arm->branch, arm->binding);
    }

    analyze_match_arm(analyzer, ast, arm, continuation_nodes, continuation_start, result_type, initial_states);
    update_ownership_states(&states);
    merge_ownership_outcome(merged_states, &states, merged_initialized);
    free_ownership_states(&states);
}

static void analyze_match_arms(Analyzer* analyzer, ASTNode* ast, Vector* continuation_nodes, size_t continuation_start,
    DataType* result_type, Vector* initial_states, Vector* merged_states, bool* merged_initialized)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        analyze_match_arm_ownership(analyzer, ast, arm, continuation_nodes, continuation_start, result_type,
            initial_states, merged_states, merged_initialized);
    }
}

static void analyze_match_subject(Analyzer* analyzer, ASTNode* ast)
{
    if (!ast->match.subject) {
        return;
    }

    analyze_node(analyzer, ast->match.subject);
}

static void analyze_match_default(Analyzer* analyzer, ASTNode* ast, Vector* continuation_nodes,
    size_t continuation_start, DataType* result_type, Vector* initial_states,
    Vector* merged_states, bool* merged_initialized)
{
    if (!ast->match.default_branch) {
        return;
    }

    Vector states;

    copy_ownership_state(&states, initial_states);
    apply_ownership_states(&states);
    restore_match_continuation(analyzer, continuation_nodes, continuation_start);
    release_references_dead_on_branch(analyzer, &states, ast->match.default_branch, NULL);

    DataType branch_type = analyze_scoped_branch(analyzer, ast->match.default_branch, NULL, ast->match.type_id);
    
    merge_match_result(ast, ast->match.default_branch, branch_type, result_type);
    update_ownership_states(&states);
    merge_ownership_outcome(merged_states, &states, merged_initialized);
    free_ownership_states(&states);
}

static void finalize_match_type(ASTNode* ast, DataType result_type)
{
    if (!ast->match.expression) {
        ast->match.type_id = TypeVoid;

        return;
    }

    if (!match_is_exhaustive(ast)) {
        semantic_token_error("Match expression must be exhaustive ", ast->match.token);
    }

    ast->match.type_id = result_type;
}

static void finalize_match_ownership(ASTNode* ast, Vector* initial_states,
    Vector* merged_states, bool* merged_initialized)
{
    if (!match_is_exhaustive(ast)) {
        merge_ownership_outcome(merged_states, initial_states, merged_initialized);
    }

    apply_ownership_states(merged_states);
    free_ownership_states(merged_states);
    free_ownership_states(initial_states);
}

static void validate_match_expression(ASTNode* ast)
{
    if (!ast->match.expression) {
        return;
    }

    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        validate_expression_branch(arm->branch, ast->match.token);
    }

    if (ast->match.default_branch) {
        validate_expression_branch(ast->match.default_branch, ast->match.token);
    }
}

static void analyze_match(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuation_nodes = analyzer->current_nodes;
    size_t continuation_start = analyzer->next_node;
    DataType result_type = TypeUnknown;
    Vector initial_states;
    Vector merged_states;
    bool merged_initialized = false;

    validate_match_expression(ast);
    analyze_match_subject(analyzer, ast);
    capture_ownership_state(&initial_states, analyzer->current_scope);
    analyze_match_arms(analyzer, ast, continuation_nodes, continuation_start, &result_type,
        &initial_states, &merged_states, &merged_initialized);
    analyze_match_default(analyzer, ast, continuation_nodes, continuation_start, &result_type,
        &initial_states, &merged_states, &merged_initialized);
    restore_match_continuation(analyzer, continuation_nodes, continuation_start);
    finalize_match_ownership(ast, &initial_states, &merged_states, &merged_initialized);
    finalize_match_type(ast, result_type);
}

static DataType get_float_binary_type(DataType left_type, DataType right_type)
{
    if (left_type == TypeF64 || right_type == TypeF64) {
        return TypeF64;
    }

    return TypeF32;
}

static void apply_binary_float_literal_types(ASTNode* left, ASTNode* right)
{
    DataType left_type = get_node_data_type(left);
    DataType right_type = get_node_data_type(right);

    if (!is_float_type(left_type) || !is_float_type(right_type)) {
        return;
    }

    DataType type = get_float_binary_type(left_type, right_type);

    if (!get_float_literal(left)) {
        type = left_type;
    }

    if (!get_float_literal(right)) {
        type = right_type;
    }

    apply_literal_type(left, type);
    apply_literal_type(right, type);
}

static void apply_binary_integer_literal_types(ASTNode* left, ASTNode* right)
{
    DataType left_type = get_node_data_type(left);
    DataType right_type = get_node_data_type(right);
    bool negative;
    ASTNode* left_literal = get_integer_literal(left, &negative);
    ASTNode* right_literal = get_integer_literal(right, &negative);
    bool same_signedness = is_signed_integer_type(left_type) == is_signed_integer_type(right_type);
    bool widen_left = same_signedness && can_implicitly_widen_integer(left_type, right_type);

    if (left_literal && right_literal && widen_left) {
        apply_integer_literal_type(left, right_type);

        left_type = right_type;
    }

    if (is_integer_type(left_type)) {
        apply_integer_literal_type(right, left_type);

        right_type = get_node_data_type(right);
    }

    if (is_integer_type(right_type)) {
        apply_integer_literal_type(left, right_type);
    }
}

static void analyze_binary(Analyzer* analyzer, ASTNode* ast)
{
    analyze_node(analyzer, ast->binary.left_expr);
    analyze_node(analyzer, ast->binary.right_expr);

    DataType left_type = get_node_data_type(ast->binary.left_expr);
    DataType right_type = get_node_data_type(ast->binary.right_expr);
    TokenType operator_type = ast->binary.operator.type;

    bool comparison = is_comparison_token(operator_type) || is_equality_token(operator_type);

    if (comparison) {
        apply_binary_float_literal_types(ast->binary.left_expr, ast->binary.right_expr);
    }

    bool left_text = left_type == TypeChar || left_type == TypeString;
    bool right_text = right_type == TypeChar || right_type == TypeString;

    if (ast->binary.operator.type == TokenPlus && left_text && right_text) {
        ast->binary.type_id = TypeString;

        return;
    }

    apply_binary_integer_literal_types(ast->binary.left_expr, ast->binary.right_expr);

    left_type = get_node_data_type(ast->binary.left_expr);
    right_type = get_node_data_type(ast->binary.right_expr);
    DataType common_type = get_common_numeric_type(left_type, right_type);
    bool float_operands = is_float_type(common_type);

    if (left_type != right_type && common_type == TypeUnknown) {
        binary_type_error(ast->binary.operator, left_type, right_type);
    }

    if (common_type == TypeUnknown) {
        common_type = left_type;
    }

    ast->binary.operand_type = common_type;

    if (left_type == TypeString) {
        semantic_error("Invalid operator for string operands", ast->binary.operator);
    }

    if (left_type == TypeChar && !is_character_comparison_token(ast->binary.operator.type)) {
        semantic_error("Invalid operator for char operands", ast->binary.operator);
    }

    if (is_logical_token(ast->binary.operator.type) && left_type != TypeBool) {
        semantic_error("Logical operators require bool operands", ast->binary.operator);
    }

    if (is_comparison_token(ast->binary.operator.type) && !is_integer_type(left_type) && !float_operands && left_type != TypeChar) {
        semantic_error("Comparison operators require compatible operand types", ast->binary.operator);
    }

    if (ast->binary.operator.type == TokenPower && left_type != TypeI32) {
        semantic_error("Invalid operands for binary operator", ast->binary.operator);
    }

    if (float_operands && !is_term_token(ast->binary.operator.type)
        && !is_factor_token(ast->binary.operator.type) && !is_comparison_token(ast->binary.operator.type)
        && !is_equality_token(ast->binary.operator.type)) {
        semantic_error("Invalid operands for binary operator", ast->binary.operator);
    }

    ast->binary.type_id = common_type;

    if (ast->binary.operator.type == TokenSpaceship) {
        ast->binary.type_id = TypeI32;
    }

    if (is_bool_operator_token(ast->binary.operator.type)) {
        ast->binary.type_id = TypeBool;
    }
}

static void track_variable_read(ASTNode* symbol)
{
    if (!is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.read_count++;
}

static void validate_constant_binding(Analyzer* analyzer, ASTNode* symbol, Token token)
{
    if (!analyzer->constant_scope || (is_variable_definition(symbol) && symbol->variable_definition.constant)) {
        return;
    }

    Scope* scope = get_scope(symbol);

    if (scope->level > analyzer->constant_scope->level) {
        return;
    }

    fatal_token_error(token, " in constant expression", "Cannot access runtime binding ");
}

static void validate_constant_reference(Analyzer* analyzer, ASTNode* symbol, Token token)
{
    if (!analyzer->constant_scope || !symbol) {
        return;
    }

    Scope* scope = get_scope(symbol);
    bool called_function = analyzer->function_path != analyzer->constant_functions;

    if (scope->level > analyzer->constant_scope->level || (called_function && !is_top_level_scope(scope))) {
        return;
    }

    Token binding = token;

    if (symbol->type == ASTParameter) {
        binding = symbol->parameter.token;
    }

    if (is_variable_definition(symbol)) {
        binding = symbol->variable_definition.token;
    }

    binding.line = token.line;
    binding.column = token.column;

    fatal_token_error(binding, " in constant expression", "Cannot reference runtime binding ");
}

static void validate_constant_mutation(ASTNode* symbol, ReferenceType type, Token token)
{
    if (type == RefExclusive && is_variable_definition(symbol) && symbol->variable_definition.constant) {
        semantic_token_error("Cannot mutate constant ", token);
    }
}

static void analyze_variable(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->variable.id);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || !is_variable_type(symbol)) {
        symbol_error(" is undefined", ast->variable.token);
    }

    validate_constant_binding(analyzer, symbol, ast->variable.token);

    if (!is_initialized(symbol)) {
        symbol_error(" is uninitialized", ast->variable.token);
    }

    if (is_moved(symbol)) {
        symbol_error(" is moved", ast->variable.token);
    }

    if (get_reference_type(symbol) == RefNone
        && has_exclusive_access(symbol)) {
        binding_access_error("Cannot read binding ", ast->variable.token, " while exclusive access is active");
    }

    ast->variable.scope = analyzer->current_scope;
    ast->variable.symbol = symbol;
    track_variable_read(symbol);
}

static ASTNode* get_reference_variable(ASTNode* ast)
{
    if (!is_variable(ast->prefix.expr)) {
        semantic_error("References require a binding", ast->prefix.operator);
    }

    return ast->prefix.expr;
}

static ASTNode* find_reference_symbol(Analyzer* analyzer, ASTNode* variable)
{
    StringView id = get_string_view(variable->variable.id);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || !is_variable_type(symbol)) {
        symbol_error(" is undefined", variable->variable.token);
    }

    if (!is_initialized(symbol)) {
        symbol_error(" is uninitialized", variable->variable.token);
    }

    if (is_moved(symbol)) {
        symbol_error(" is moved", variable->variable.token);
    }

    return symbol;
}

static void validate_reference_access(ASTNode* ast, ASTNode* variable, ASTNode* symbol)
{
    ReferenceType requested = get_reference_type(ast);
    ReferenceType existing = get_reference_type(symbol);

    validate_constant_mutation(symbol, requested, variable->variable.token);

    if (existing != RefNone) {
        semantic_error("Cannot create a reference to another reference", ast->prefix.operator);
    }

    if (requested == RefShared && has_exclusive_access(symbol)) {
        binding_access_error("Cannot share binding ", variable->variable.token, " while exclusive access is active");
    }

    if (requested == RefExclusive
        && (has_exclusive_access(symbol) || *get_shared_access_count(symbol))) {
        binding_access_error("Cannot take exclusive access to binding ", variable->variable.token, " while another access is active");
    }
}

static void mark_address_taken(ASTNode* symbol)
{
    if (!symbol || !is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.address_taken = true;
}

static void analyze_reference(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* variable = get_reference_variable(ast);
    ASTNode* symbol = find_reference_symbol(analyzer, variable);

    validate_constant_binding(analyzer, symbol, variable->variable.token);
    validate_constant_reference(analyzer, symbol, variable->variable.token);
    validate_reference_access(ast, variable, symbol);
    mark_address_taken(symbol);

    variable->variable.scope = analyzer->current_scope;
    variable->variable.symbol = symbol;
    Vector* origins = get_reference_origins(ast);
    
    add_reference_origin(origins, symbol);
    track_variable_read(symbol);
}

static void analyze_parameter_default(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* value = ast->parameter.default_value;

    if (!value) {
        return;
    }

    if (ast->parameter.reference_type != RefNone) {
        semantic_token_error("Reference parameter cannot have a default: ", ast->parameter.token);
    }

    Scope* previous_scope = analyzer->current_scope;
    analyzer->current_scope = previous_scope->parent;
    apply_declared_type(value, ast->parameter.type_id, RefNone);
    analyze_node(analyzer, value);
    analyzer->current_scope = previous_scope;

    DataType source = get_node_data_type(value);
    DataType destination = ast->parameter.type_id;
    bool compatible = source == destination || can_implicitly_widen_type(source, destination);

    if (!compatible) {
        semantic_token_error("Default value type does not match parameter ", ast->parameter.token);
    }
}

static void analyze_parameter(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->parameter.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Parameter", ast->parameter.token);
    }

    ast->parameter.scope = analyzer->current_scope;

    analyze_parameter_default(analyzer, ast);

    if (get_reference_type(ast) != RefNone) {
        Vector* origins = get_reference_origins(ast);
        add_reference_origin(origins, ast);
    }

    set_local_symbol(analyzer->current_scope, id, ast);
}

static void validate_typed_variadic_argument(ASTNode* argument, DataType type, Token token)
{
    apply_literal_type(argument, type);

    DataType argument_type = get_node_data_type(argument);

    if (argument_type != type && !can_implicitly_widen_type(argument_type, type)) {
        semantic_token_error("Variadic arguments must convert losslessly to the parameter type ", token);
    }
}

static void validate_typed_variadic_arguments(Vector* args, const Builtin* builtin, Token token)
{
    if (!args->count) {
        semantic_token_error("Numeric variadic builtins require at least one argument ", token);
    }

    for (size_t i = 0; i < args->count; i++) {
        validate_typed_variadic_argument(args->data[i], builtin->params[0], token);
    }
}

static void validate_variadic_arguments(Vector* args, const Builtin* builtin, Token token)
{
    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        validate_typed_variadic_arguments(args, builtin, token);

        return;
    }

    char* message = "Invalid value in string construction ";

    if (builtin->entry == builtin_print) {
        message = "Invalid value in print ";
    }

    for (size_t i = 0; i < args->count; i++) {
        ASTNode* argument = args->data[i];
        DataType type = get_node_data_type(argument);
        bool valid = is_printable_type(type);

        if (!valid) {
            semantic_token_error(message, token);
        }
    }
}

static void validate_builtin_call(ASTNode* ast, const Builtin* builtin, Token token)
{
    size_t count = count_vector(&ast->function_call.args);

    if (builtin->param_count < 0) {
        validate_variadic_arguments(&ast->function_call.args, builtin, token);

        return;
    }

    if (count != (size_t)builtin->param_count) {
        semantic_token_error("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = get_vector_at(&ast->function_call.args, i);
        apply_literal_type(arg, builtin->params[i]);

        DataType argument_type = get_node_data_type(arg);
        DataType parameter_type = builtin->params[i];

        if (argument_type != parameter_type
            && !can_implicitly_widen_type(argument_type, parameter_type)) {
            semantic_token_error("Invalid arguments to function ", token);
        }
    }
}

static void convert_builtin_call(ASTNode* ast, const Builtin* builtin)
{
    Vector args = ast->function_call.args;
    Token token = ast->function_call.token;

    free_string_object(ast->function_call.id);

    ast->type = ASTBuiltinCall;
    ast->builtin_call.builtin = builtin;
    ast->builtin_call.args = args;
    ast->builtin_call.token = token;
}

static bool is_numeric_literal_argument(ASTNode* argument)
{
    bool negative;
    ASTNode* integer = get_integer_literal(argument, &negative);
    ASTNode* floating = get_float_literal(argument);

    return integer || floating;
}

static DataType get_nonliteral_argument_type(ASTNode* argument)
{
    bool literal = is_numeric_literal_argument(argument);

    if (literal) {
        return TypeUnknown;
    }

    return get_node_data_type(argument);
}

static bool variadic_argument_fits_type(ASTNode* argument, DataType type)
{
    bool negative;
    ASTNode* integer = get_integer_literal(argument, &negative);

    if (integer) {
        bool fits_integer = integer_literal_fits_type(integer->integer_literal.value, negative, type);
        bool fits_float = integer_literal_fits_float(integer->integer_literal.value, type);

        return fits_integer || fits_float;
    }

    ASTNode* floating = get_float_literal(argument);

    if (floating) {
        bool numeric = is_float_type(type);
        bool fits = type == TypeF64 || floating->float_literal.value <= FLT_MAX;

        return numeric && fits;
    }

    DataType argument_type = get_node_data_type(argument);

    return argument_type == type || can_implicitly_widen_type(argument_type, type);
}

static bool typed_variadic_call_matches(Vector* args, const Builtin* builtin)
{
    if (!builtin || builtin->param_count != BUILTIN_TYPED_VARIADIC) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < args->count && matches; i++) {
        matches = variadic_argument_fits_type(args->data[i], builtin->params[0]);
    }

    return matches;
}

static const Builtin* resolve_contextual_variadic_call(ASTNode* ast)
{
    Vector* args = &ast->function_call.args;
    DataType type = TypeUnknown;

    for (size_t i = 0; i < args->count && type == TypeUnknown; i++) {
        type = get_nonliteral_argument_type(args->data[i]);
    }

    if (type == TypeUnknown) {
        return NULL;
    }

    const Builtin* builtin = resolve_builtin(ast->function_call.id->chars, &type, 1);
    bool matches = typed_variadic_call_matches(args, builtin);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static DataType get_builtin_argument_type(ASTNode* argument)
{
    DataType type = get_node_data_type(argument);

    if (type == TypeUnknown) {
        return TypeU64;
    }

    return type;
}

static const Builtin* resolve_builtin_call(ASTNode* ast)
{
    const Builtin* contextual = resolve_contextual_variadic_call(ast);

    if (contextual) {
        return contextual;
    }

    size_t count = ast->function_call.args.count;
    DataType* argument_types = NULL;

    if (count > SIZE_MAX / sizeof(DataType)) {
        out_of_memory_error();
    }

    if (count) {
        argument_types = allocate_memory(count * sizeof(DataType));
    }

    for (size_t i = 0; i < count; i++) {
        argument_types[i] = get_builtin_argument_type(ast->function_call.args.data[i]);
    }

    const Builtin* builtin = resolve_builtin(ast->function_call.id->chars, argument_types, count);

    free_memory(argument_types);

    return builtin;
}

static void validate_builtin_resolution(ASTNode* ast, const Builtin* builtin)
{
    if (builtin) {
        return;
    }

    if (is_builtin_name(ast->function_call.id->chars)) {
        invalid_builtin_arguments_error(ast->function_call.token);
    }

    symbol_error(" is undefined", ast->function_call.token);
}

static void analyze_builtin_call(ASTNode* ast)
{
    const Builtin* builtin = resolve_builtin_call(ast);

    validate_builtin_resolution(ast, builtin);
    validate_builtin_call(ast, builtin, ast->function_call.token);
    convert_builtin_call(ast, builtin);
}

static bool has_required_omitted_parameter(ASTNode* callee, size_t count, size_t param_count)
{
    if (count >= param_count) {
        return false;
    }

    ASTNode* first_omitted = get_vector_at(&callee->function_definition.params, count);

    return !first_omitted->parameter.default_value;
}

static void validate_function_call(ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = count_vector(&caller->function_call.args);
    size_t param_count = count_vector(&callee->function_definition.params);

    if (count > param_count || has_required_omitted_parameter(callee, count, param_count)) {
        semantic_token_error("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = get_vector_at(&caller->function_call.args, i);
        ASTNode* param = get_vector_at(&callee->function_definition.params, i);
        apply_literal_type(arg, param->parameter.type_id);

        ReferenceType argument_type = get_reference_type(arg);
        ReferenceType parameter_type = get_reference_type(param);
        bool value_parameter = parameter_type == RefNone;
        bool implicit_access = argument_type == RefNone;
        bool compatible_reference = value_parameter || implicit_access || argument_type == parameter_type
            || (argument_type == RefExclusive && parameter_type == RefShared);
        DataType argument_value_type = get_node_data_type(arg);
        DataType parameter_value_type = param->parameter.type_id;
        bool compatible_value = argument_value_type == parameter_value_type
            || (argument_type == RefNone && parameter_type == RefNone
                && can_implicitly_widen_type(argument_value_type, parameter_value_type));

        if (!compatible_value || !compatible_reference) {
            semantic_token_error("Invalid arguments to function ", token);
        }
    }
}

static void validate_global_binding_access(Analyzer* analyzer, ASTNode* origin, ReferenceType type, Token token)
{
    validate_constant_mutation(origin, type, token);

    if (!origin || !is_top_level_scope(get_scope(origin))) {
        return;
    }

    validate_constant_binding(analyzer, origin, token);

    if (type == RefShared && has_exclusive_access(origin)) {
        binding_access_error("Function requires shared access to binding ", token, " while exclusive access is active");
    }

    if (type == RefExclusive
        && (has_exclusive_access(origin) || *get_shared_access_count(origin))) {
        binding_access_error("Function requires exclusive access to binding ", token, " while another access is active");
    }
}

static bool reference_argument_requires_binding(ReferenceType type)
{
    return type == RefExclusive;
}

static void validate_call_argument_access(Analyzer* analyzer, ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = count_vector(&caller->function_call.args);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_vector_at(&callee->function_definition.params, i);
        ReferenceType type = get_reference_type(param);

        if (type == RefNone) {
            continue;
        }

        ASTNode* arg = get_vector_at(&caller->function_call.args, i);

        if (get_reference_type(arg) != RefNone) {
            continue;
        }

        ASTNode* origin = reference_origin_from_expression(arg);

        if (!origin && is_variable(arg)) {
            origin = get_reference_origin_or_self(arg->variable.symbol);
        }

        if (!origin && reference_argument_requires_binding(type)) {
            semantic_token_error("Reference access requires a binding for ", token);
        }

        validate_global_binding_access(analyzer, origin, type, token);
        validate_constant_reference(analyzer, origin, token);
        mark_address_taken(origin);
    }
}

static bool function_in_path(FunctionPath* path, ASTNode* function)
{
    if (!path || path->function == function) {
        return path != NULL;
    }

    return function_in_path(path->previous, function);
}

static void validate_global_access_in_nodes(Analyzer* analyzer, Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        validate_node_global_access(analyzer, node);
    }
}

static void validate_function_global_access(Analyzer* analyzer, ASTNode* function)
{
    if (analyzer->constant_scope && function_in_path(analyzer->constant_functions, function)) {
        semantic_token_error("Cannot evaluate incomplete function ", function->function_definition.token);
    }

    if (function_in_path(analyzer->function_path, function)) {
        return;
    }

    FunctionPath path = {function, analyzer->function_path};
    analyzer->function_path = &path;

    validate_global_access_in_nodes(analyzer, &function->function_definition.body->compound.statements);

    analyzer->function_path = path.previous;
}

static void validate_branch_global_access(Analyzer* analyzer, ASTNode* branch)
{
    if (!branch) {
        return;
    }

    if (branch->type == ASTConditional) {
        validate_node_global_access(analyzer, branch);

        return;
    }

    validate_global_access_in_nodes(analyzer, &branch->compound.statements);
}

static void validate_match_arm_global_access(Analyzer* analyzer, MatchArm* arm)
{
    if (arm->pattern) {
        validate_node_global_access(analyzer, arm->pattern);
    }

    if (arm->guard) {
        validate_node_global_access(analyzer, arm->guard);
    }

    validate_branch_global_access(analyzer, arm->branch);
}

static void validate_match_global_access(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->match.subject) {
        validate_node_global_access(analyzer, ast->match.subject);
    }

    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);

        validate_match_arm_global_access(analyzer, arm);
    }

    validate_branch_global_access(analyzer, ast->match.default_branch);
}

static void validate_prefix_global_access(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* origin = reference_origin_from_expression(ast);
    ReferenceType type = get_reference_type(ast);

    if (type != RefNone) {
        validate_constant_reference(analyzer, origin, ast->prefix.operator);
    }

    validate_global_binding_access(analyzer, origin, type, ast->prefix.operator);
    validate_node_global_access(analyzer, ast->prefix.expr);
}

static void validate_node_global_access(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case ASTAssignment: {
            ASTNode* origin = get_reference_origin_or_self(ast->assignment.symbol);
            validate_global_binding_access(analyzer, origin, RefExclusive, ast->assignment.token);
            validate_node_global_access(analyzer, ast->assignment.expr);
            break;
        }
        case ASTBinary:
            validate_node_global_access(analyzer, ast->binary.left_expr);
            validate_node_global_access(analyzer, ast->binary.right_expr);
            break;
        case ASTBuiltinCall:
            validate_global_access_in_nodes(analyzer, &ast->builtin_call.args);
            break;
        case ASTWhile:
            validate_node_global_access(analyzer, ast->while_statement.condition);
            validate_branch_global_access(analyzer, ast->while_statement.body);
            break;
        case ASTConditional:
            validate_node_global_access(analyzer, ast->conditional.condition);
            validate_branch_global_access(analyzer, ast->conditional.then_branch);
            validate_branch_global_access(analyzer, ast->conditional.else_branch);
            break;
        case ASTMatch:
            validate_match_global_access(analyzer, ast);
            break;
        case ASTConversion:
            validate_node_global_access(analyzer, ast->conversion.expr);
            break;
        case ASTFunctionCall:
            validate_call_argument_access(analyzer, ast, ast->function_call.symbol, ast->function_call.token);
            validate_global_access_in_nodes(analyzer, &ast->function_call.args);
            validate_function_global_access(analyzer, ast->function_call.symbol);
            break;
        case ASTPrefix:
            validate_prefix_global_access(analyzer, ast);
            break;
        case ASTReturn:
            validate_node_global_access(analyzer, ast->return_statement.expr);
            break;
        case ASTVariable: {
            ASTNode* origin = get_reference_origin_or_self(ast->variable.symbol);
            validate_global_binding_access(analyzer, origin, RefShared, ast->variable.token);
            break;
        }
        case ASTVariableDefinition:
            validate_node_global_access(analyzer, ast->variable_definition.expr);
            break;
        default:
            break;
    }
}

static ReferenceType get_argument_reference_type(ASTNode* argument, ASTNode* parameter)
{
    ReferenceType type = get_reference_type(argument);

    if (type != RefNone) {
        return type;
    }

    return get_reference_type(parameter);
}

static ASTNode* get_argument_reference_origin(ASTNode* argument)
{
    ASTNode* origin = reference_origin_from_expression(argument);

    if (origin || !is_variable(argument)) {
        return origin;
    }

    return get_reference_origin_or_self(argument->variable.symbol);
}

static bool reference_access_conflicts(ASTNode* left_origin, ReferenceType left_type,
    ASTNode* right, ASTNode* right_parameter)
{
    ReferenceType right_type = get_argument_reference_type(right, right_parameter);

    if (left_type != RefExclusive && right_type != RefExclusive) {
        return false;
    }

    Vector* right_origins = reference_origins_from_expression(right);
    if (contains_reference_origin(right_origins, left_origin)) {
        return true;
    }

    ASTNode* right_origin = get_argument_reference_origin(right);

    return left_origin == right_origin;
}

static void validate_argument_origin_access(Vector* arguments, Vector* parameters,
    size_t position, ASTNode* left_origin, ReferenceType left_type, Token token)
{
    size_t count = count_vector(arguments);

    for (size_t i = position + 1; i < count; i++) {
        ASTNode* right = get_vector_at(arguments, i);
        ASTNode* right_parameter = get_vector_at(parameters, i);

        if (!reference_access_conflicts(left_origin, left_type, right, right_parameter)) {
            continue;
        }

        semantic_token_error("Conflicting reference access in call ", token);
    }
}

static void validate_argument_access(Vector* arguments, Vector* parameters,
    size_t position, Token token)
{
    ASTNode* left = get_vector_at(arguments, position);
    ASTNode* left_parameter = get_vector_at(parameters, position);
    ReferenceType left_type = get_argument_reference_type(left, left_parameter);

    if (left_type == RefNone) {
        return;
    }

    Vector* origins = reference_origins_from_expression(left);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);
        validate_argument_origin_access(arguments, parameters, position, origin, left_type, token);
    }

    if (count) {
        return;
    }

    ASTNode* origin = get_argument_reference_origin(left);
    if (origin) {
        validate_argument_origin_access(arguments, parameters, position, origin, left_type, token);
    }
}

static void validate_argument_accesses(ASTNode* caller, ASTNode* callee)
{
    Vector* arguments = &caller->function_call.args;
    Vector* parameters = &callee->function_definition.params;
    size_t count = count_vector(arguments);

    for (size_t i = 0; i < count; i++) {
        validate_argument_access(arguments, parameters, i, caller->function_call.token);
    }
}

static void set_call_reference_origins(ASTNode* caller, ASTNode* callee)
{
    ASTNode* returned = callee->function_definition.return_reference_origin;

    if (!returned || !is_parameter(returned)) {
        Vector* origins = get_reference_origins(caller);
        add_reference_origin(origins, returned);

        return;
    }

    size_t count = count_vector(&callee->function_definition.params);

    for (size_t i = 0; i < count; i++) {
        if (get_vector_at(&callee->function_definition.params, i) != returned) {
            continue;
        }

        ASTNode* argument = get_vector_at(&caller->function_call.args, i);
        set_reference_origins_from_expression(caller, argument);

        return;
    }
}

static void infer_conversion_literal(ASTNode* ast)
{
    DataType type = get_token_data_type(ast->function_call.token.type);
    size_t count = count_vector(&ast->function_call.args);

    if ((!is_integer_type(type) && !is_float_type(type) && type != TypeChar) || count != 1) {
        return;
    }

    ASTNode* expression = ast->function_call.args.data[0];
    DataType source = get_node_data_type(expression);

    if (source == TypeUnknown && expression->type == ASTInteger) {
        apply_integer_literal_type(expression, TypeU64);
    }
}

static void analyze_type_conversion(ASTNode* ast)
{
    Token token = ast->function_call.token;
    DataType destination = get_token_data_type(token.type);
    size_t count = count_vector(&ast->function_call.args);

    if (count != 1) {
        semantic_error("Explicit type conversion requires exactly one argument", token);
    }

    ASTNode* expression = ast->function_call.args.data[0];
    DataType source = get_node_data_type(expression);
    bool supported = can_explicitly_convert_type(source, destination);
    ReferenceType reference = get_reference_type(expression);

    if (!supported) {
        unsupported_explicit_conversion_error(token, source, destination);
    }

    if (reference != RefNone) {
        semantic_error("Explicit type conversions require a value", token);
    }

    free_string_object(ast->function_call.id);
    free_vector(&ast->function_call.args);
    free_vector(&ast->function_call.reference_origins);

    ast->type = ASTConversion;
    ast->conversion.expr = expression;
    ast->conversion.token = token;
    ast->conversion.type_id = destination;
}

static void analyze_function_call(Analyzer* analyzer, ASTNode* ast)
{
    infer_conversion_literal(ast);
    analyze_expression_nodes(analyzer, &ast->function_call.args);

    DataType called_type = get_token_data_type(ast->function_call.token.type);

    if (is_integer_type(called_type) || is_float_type(called_type) || called_type == TypeChar) {
        analyze_type_conversion(ast);

        return;
    }

    StringView id = get_string_view(ast->function_call.id);
    ASTNode* symbol = find_symbol(analyzer, id);
    
    if (!symbol) {
        analyze_builtin_call(ast);
        return;
    }

    if (!is_function_definition(symbol)) {
        symbol_error(" is undefined", ast->function_call.token);
    }

    ast->function_call.scope = analyzer->current_scope;
    ast->function_call.symbol = symbol;
    validate_function_call(ast, symbol, ast->function_call.token);
    validate_call_argument_access(analyzer, ast, symbol, ast->function_call.token);
    validate_argument_accesses(ast, symbol);
    validate_function_global_access(analyzer, symbol);
    set_call_reference_origins(ast, symbol);
}

static ASTNode* find_function_return(Vector* statements)
{
    size_t count = count_vector(statements);

    for (size_t i = 0; i < count; i++) {
        ASTNode* statement = get_vector_at(statements, i);
        ASTNode* result = find_node_return(statement);

        if (result) {
            return result;
        }
    }

    return NULL;
}

static ASTNode* find_conditional_return(ASTNode* ast)
{
    ASTNode* result = find_node_return(ast->conditional.then_branch);

    if (result) {
        return result;
    }

    return find_node_return(ast->conditional.else_branch);
}

static ASTNode* find_match_return(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        ASTNode* result = find_node_return(arm->branch);

        if (result) {
            return result;
        }
    }

    return find_node_return(ast->match.default_branch);
}

static ASTNode* find_node_return(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTReturn:
            return ast;
        case ASTCompound:
            return find_function_return(&ast->compound.statements);
        case ASTWhile:
            return find_node_return(ast->while_statement.body);
        case ASTConditional:
            return find_conditional_return(ast);
        case ASTMatch:
            return find_match_return(ast);
        default:
            return NULL;
    }
}

static DataType get_function_return_value_type(Vector* statements)
{
    ASTNode* result = find_function_return(statements);

    if (!result) {
        return TypeVoid;
    }

    return get_node_data_type(result->return_statement.expr);
}

static void validate_function_return_value_type(ASTNode* ast, DataType type)
{
    DataType return_type = ast->function_definition.return_type_id;

    bool value_return = ast->function_definition.return_reference_type == RefNone;

    if (return_type != type
        && (!value_return || !can_implicitly_widen_type(type, return_type))) {
        semantic_token_error("Invalid return type for function ", ast->function_definition.token);
    }
}

static void set_parameter_positions(ASTNode* ast)
{
    size_t count = count_vector(&ast->function_definition.params);
    size_t position = 0;

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_vector_at(&ast->function_definition.params, i);
        param->parameter.position = position;
        position += get_reference_type(param) == RefNone ? get_type_slot_count(get_node_data_type(param)) : 1;
    }
}

static void resolve_function_return_value_type(ASTNode* ast)
{
    DataType type = get_function_return_value_type(&ast->function_definition.body->compound.statements);

    if (ast->function_definition.has_explicit_return_type) {
        validate_function_return_value_type(ast, type);
        return;
    }

    ast->function_definition.return_type_id = type;
}

static void validate_parameter_order(ASTNode* parameter, bool* has_default)
{
    if (parameter->parameter.default_value) {
        *has_default = true;

        return;
    }

    if (*has_default) {
        semantic_token_error("Required parameter cannot follow a default parameter: ", parameter->parameter.token);
    }
}

static void analyze_parameters(Analyzer* analyzer, Vector* parameters)
{
    bool has_default = false;

    for (size_t i = 0; i < parameters->count; i++) {
        ASTNode* parameter = parameters->data[i];

        validate_parameter_order(parameter, &has_default);
        analyze_node(analyzer, parameter);
    }
}

static void analyze_function(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->function_definition.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Function", ast->function_definition.token);
    }

    Scope* parent = analyzer->current_scope;
    ASTNode* previous_function = analyzer->function;
    LoopPath* previous_loop = analyzer->loop_path;
    Scope* scope = create_scope(parent);
    scope->frame_scope = scope;
    scope->local_offset = 0;
    scope->max_local_count = 0;
    ast->function_definition.scope = scope;
    ast->function_definition.body->compound.scope = scope;
    analyzer->current_scope = scope;
    analyzer->function = ast;
    analyzer->loop_path = NULL;

    set_local_symbol(scope, id, ast);
    analyze_parameters(analyzer, &ast->function_definition.params);
    set_parameter_positions(ast);
    set_local_symbol(parent, id, ast);

    FunctionPath path = {ast, analyzer->function_path};
    analyzer->function_path = &path;

    analyze_expression_nodes(analyzer, &ast->function_definition.body->compound.statements);

    analyzer->function_path = path.previous;
    analyzer->current_scope = parent;
    analyzer->function = previous_function;
    analyzer->loop_path = previous_loop;

    resolve_function_return_value_type(ast);
}

static ASTNode* find_assignment_symbol(Analyzer* analyzer, ASTNode* ast)
{
    Token token = ast->assignment.token;
    StringView id = create_string_view(token.chars, token.length);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || !is_variable_type(symbol)) {
        symbol_error(" is undefined", ast->assignment.token);
    }

    return symbol;
}

static void validate_compound_assignment_target(ASTNode* ast, ASTNode* symbol)
{
    if (ast->assignment.operator.type == TokenEqual) {
        return;
    }

    if (!is_initialized(symbol)) {
        symbol_error(" is uninitialized", ast->assignment.token);
    }

    if (get_reference_type(symbol) == RefShared) {
        semantic_token_error("Cannot mutate through shared reference ", ast->assignment.token);
    }
}

static void validate_assignment_target(ASTNode* ast, ASTNode* symbol)
{
    Token token = ast->assignment.token;

    if (is_variable_definition(symbol) && symbol->variable_definition.constant) {
        semantic_token_error("Cannot reassign constant ", token);
    }

    if (is_moved(symbol)) {
        symbol_error(" is moved", token);
    }

    if (get_reference_type(symbol) == RefNone && has_exclusive_access(symbol)) {
        binding_access_error("Cannot mutate binding ", token, " while exclusive access is active");
    }

    if (get_reference_type(symbol) == RefNone && *get_shared_access_count(symbol)) {
        binding_access_error("Cannot mutate binding ", token, " while shared access is active");
    }

    if (get_reference_type(symbol) == RefNone && is_variable_definition(symbol)
        && symbol->variable_definition.fixed && is_initialized(symbol)) {
        semantic_token_error("Cannot reassign fixed binding ", token);
    }
}

static void validate_assignment_type(ASTNode* ast, ASTNode* symbol)
{
    ASTNode* expression = ast->assignment.expr;
    DataType expression_type = get_node_data_type(expression);
    DataType binding_type = get_node_data_type(symbol);
    ReferenceType binding_reference_type = get_reference_type(symbol);
    ReferenceType expression_reference_type = get_reference_type(expression);

    if (ast->assignment.operator.type != TokenEqual && binding_reference_type == RefExclusive) {
        binding_reference_type = RefNone;
    }

    bool compatible_value = binding_type == expression_type || (binding_reference_type == RefNone
        && expression_reference_type == RefNone && can_implicitly_widen_type(expression_type, binding_type));

    if (compatible_value && binding_reference_type == expression_reference_type) {
        return;
    }

    if (binding_reference_type == RefNone && expression_reference_type == RefNone) {
        implicit_conversion_error(ast->assignment.token, expression_type, binding_type);
    }

    semantic_token_error("Assignment type does not match binding ", ast->assignment.token);
}

static void update_reference_binding(ASTNode* ast, ASTNode* symbol, bool initializes_binding)
{
    if (ast->assignment.operator.type != TokenEqual || get_reference_type(symbol) == RefNone) {
        return;
    }

    set_reference_origins_from_expression(symbol, ast->assignment.expr);

    if (get_reference_type(symbol) == RefExclusive && is_variable(ast->assignment.expr)) {
        transfer_exclusive_reference(ast->assignment.expr);
    } else {
        activate_reference_access(symbol);
    }

    symbol->variable_definition.reference_access_active = true;
    ast->assignment.initializes_binding = initializes_binding;
}

static void analyze_assignment(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* symbol = find_assignment_symbol(analyzer, ast);
    bool initializes_binding = !is_initialized(symbol);

    validate_constant_binding(analyzer, symbol, ast->assignment.token);
    validate_assignment_target(ast, symbol);
    validate_compound_assignment_target(ast, symbol);
    track_variable_read(symbol);

    if (ast->assignment.operator.type == TokenEqual && is_variable_definition(symbol)
        && !initializes_binding && symbol->variable_definition.reference_access_active) {
        release_reference_access(symbol);
    }

    apply_literal_type(ast->assignment.expr, get_node_data_type(symbol));
    analyze_node(analyzer, ast->assignment.expr);
    validate_assignment_type(ast, symbol);

    DataType binding_type = get_node_data_type(symbol);

    if (binding_type == TypeString && ast->assignment.operator.type != TokenEqual) {
        semantic_error("Invalid operator for string assignment", ast->assignment.operator);
    }

    if (binding_type == TypeChar && ast->assignment.operator.type != TokenEqual) {
        semantic_error("Invalid operator for char assignment", ast->assignment.operator);
    }

    if (is_float_type(get_node_data_type(symbol))
        && !is_float_assignment_token(ast->assignment.operator.type)) {
        semantic_error("Invalid operands for assignment", ast->assignment.operator);
    }

    if (ast->assignment.operator.type == TokenPowerEqual && get_node_data_type(symbol) != TypeI32) {
        semantic_error("Invalid operands for assignment", ast->assignment.operator);
    }

    ast->assignment.scope = analyzer->current_scope;
    ast->assignment.symbol = symbol;

    update_reference_binding(ast, symbol, initializes_binding);
    initialize_variable(symbol);
}

static void transfer_exclusive_reference(ASTNode* expression)
{
    ASTNode* source = expression->variable.symbol;
    set_moved(source);

    if (!is_variable_definition(source)) {
        return;
    }

    source->variable_definition.reference_access_active = false;
}

static void activate_reference_access(ASTNode* ast)
{
    Vector* origins = get_reference_origins(ast);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);

        if (ast->variable_definition.reference_type == RefShared) {
            (*get_shared_access_count(origin))++;
        } else {
            set_exclusive_access(origin, true);
        }
    }
}

static void initialize_variable_definition(Analyzer* analyzer, ASTNode* ast)
{
    ast->variable_definition.scope = analyzer->current_scope;
    ast->variable_definition.position = get_next_local_position(analyzer->current_scope);
    ast->variable_definition.initialized = false;
}

static void apply_declared_type(ASTNode* expression, DataType type, ReferenceType reference_type)
{
    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenKeywordConst) {
        apply_declared_type(expression->prefix.expr, type, reference_type);

        return;
    }

    if (type == TypeUnknown) {
        return;
    }

    apply_literal_type(expression, type);

    if (expression->type == ASTConditional) {
        expression->conditional.type_id = type;
        expression->conditional.reference_type = reference_type;
    }

    if (expression->type == ASTMatch) {
        expression->match.type_id = type;
        expression->match.reference_type = reference_type;
    }
}

static bool variable_types_match(ASTNode* ast, DataType type, ReferenceType reference_type)
{
    ReferenceType declared_reference_type = ast->variable_definition.reference_type;

    if (declared_reference_type != reference_type) {
        return false;
    }

    DataType declared_type = ast->variable_definition.type_id;
    if (declared_type == type) {
        return true;
    }

    if (reference_type != RefNone) {
        return false;
    }

    return can_implicitly_widen_type(type, declared_type);
}

static void validate_variable_type_match(ASTNode* ast, DataType type, ReferenceType reference_type)
{
    if (variable_types_match(ast, type, reference_type)) {
        return;
    }

    DataType declared_type = ast->variable_definition.type_id;
    ReferenceType declared_reference_type = ast->variable_definition.reference_type;

    if (declared_reference_type == RefNone && reference_type == RefNone) {
        implicit_conversion_error(ast->variable_definition.token, type, declared_type);
    }

    semantic_token_error("Initializer type does not match variable ", ast->variable_definition.token);
}

static void resolve_variable_type(ASTNode* ast, ASTNode* expression)
{
    DataType type = get_node_data_type(expression);
    ReferenceType reference_type = get_reference_type(expression);

    if (ast->variable_definition.type_id != TypeUnknown) {
        validate_variable_type_match(ast, type, reference_type);

        return;
    }

    if (type == TypeUnknown && expression->type == ASTFloat) {
        float_literal_requires_type_error(expression->float_literal.token);
    }

    if (type == TypeUnknown) {
        integer_literal_requires_type_error(ast->variable_definition.token);
    }

    ast->variable_definition.type_id = type;
    ast->variable_definition.reference_type = reference_type;
}

static void analyze_variable_initializer(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* expression = ast->variable_definition.expr;

    if (is_none(expression)) {
        return;
    }

    apply_declared_type(expression, ast->variable_definition.type_id, ast->variable_definition.reference_type);
    analyze_node(analyzer, expression);
    resolve_variable_type(ast, expression);

    set_reference_origins_from_expression(ast, expression);
    initialize_variable(ast);
}

static void validate_variable_type(ASTNode* ast)
{
    if (ast->variable_definition.type_id == TypeVoid) {
        semantic_token_error("Invalid type for variable ", ast->variable_definition.token);
    }
}

static void track_variable_reference(ASTNode* ast)
{
    if (ast->variable_definition.reference_type == RefNone || !ast->variable_definition.initialized) {
        return;
    }

    ASTNode* expression = ast->variable_definition.expr;
    bool transfers_exclusive = ast->variable_definition.reference_type == RefExclusive && is_variable(expression);

    if (transfers_exclusive) {
        transfer_exclusive_reference(expression);
    } else {
        activate_reference_access(ast);
    }

    ast->variable_definition.reference_access_active = true;
}

static void analyze_variable_definition(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->variable_definition.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Variable", ast->variable_definition.token);
    }

    initialize_variable_definition(analyzer, ast);
    analyze_variable_initializer(analyzer, ast);
    validate_variable_type(ast);
    set_local_variable_symbol(analyzer->current_scope, id, ast);
    track_variable_reference(ast);
}

static bool can_return_reference_origin(ASTNode* origin)
{
    if (!origin) {
        return false;
    }

    if (is_top_level_scope(get_scope(origin))) {
        return true;
    }

    return is_parameter(origin) && get_reference_type(origin) != RefNone;
}

static ReferenceType get_expected_return_reference_type(Analyzer* analyzer)
{
    if (!analyzer->function) {
        return RefNone;
    }

    return analyzer->function->function_definition.return_reference_type;
}

static void validate_return_scope(Analyzer* analyzer, ASTNode* ast)
{
    if (analyzer->current_scope->level < 2) {
        return_outside_function_error(ast->return_statement.token);
    }
}

static ReferenceType validate_return_reference_type(Analyzer* analyzer, ASTNode* ast)
{
    ReferenceType expected = get_expected_return_reference_type(analyzer);

    if (get_reference_type(ast->return_statement.expr) != expected) {
        semantic_token_error("Invalid reference return from function ", ast->return_statement.token);
    }

    return expected;
}

static void track_return_origin(Analyzer* analyzer, ASTNode* ast, ASTNode* origin)
{
    if (!can_return_reference_origin(origin)) {
        semantic_token_error("Reference to local binding cannot escape function ", ast->return_statement.token);
    }

    ASTNode* function = analyzer->function;
    ASTNode* previous = function->function_definition.return_reference_origin;

    if (previous && previous != origin) {
        semantic_token_error("Reference returns must share one lifetime in function ", ast->return_statement.token);
    }

    function->function_definition.return_reference_origin = origin;
}

static void track_return_reference_origin(Analyzer* analyzer, ASTNode* ast)
{
    Vector* origins = reference_origins_from_expression(ast->return_statement.expr);
    size_t count = count_vector(origins);

    if (!count) {
        ASTNode* origin = reference_origin_from_expression(ast->return_statement.expr);
        track_return_origin(analyzer, ast, origin);

        return;
    }

    for (size_t i = 0; i < count; i++) {
        track_return_origin(analyzer, ast, get_vector_at(origins, i));
    }
}

static void analyze_return(Analyzer* analyzer, ASTNode* ast)
{
    validate_return_scope(analyzer, ast);

    ASTNode* function = analyzer->function;

    if (function->function_definition.has_explicit_return_type) {
        apply_literal_type(ast->return_statement.expr, function->function_definition.return_type_id);
    }

    analyze_node(analyzer, ast->return_statement.expr);

    if (validate_return_reference_type(analyzer, ast) == RefNone) {
        return;
    }

    track_return_reference_origin(analyzer, ast);
}

static void infer_negated_integer_literal_type(ASTNode* ast)
{
    bool negative;
    ASTNode* literal = get_integer_literal(ast, &negative);

    if (!literal || !negative) {
        return;
    }

    if (literal->integer_literal.type_id != TypeUnknown) {
        return;
    }

    apply_integer_literal_type(ast, TypeI64);
}

static void analyze_constant_expression(Analyzer* analyzer, ASTNode* ast)
{
    Scope* previous = analyzer->constant_scope;
    FunctionPath* previous_functions = analyzer->constant_functions;
    analyzer->constant_scope = analyzer->current_scope;
    analyzer->constant_functions = analyzer->function_path;

    analyze_node(analyzer, ast->prefix.expr);

    analyzer->constant_scope = previous;
    analyzer->constant_functions = previous_functions;

    if (get_reference_type(ast->prefix.expr) != RefNone || get_node_data_type(ast) == TypeVoid) {
        semantic_error("Constant expression must produce a value", ast->prefix.operator);
    }
}

static void analyze_prefix(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        analyze_constant_expression(analyzer, ast);

        return;
    }

    if (get_reference_type(ast) != RefNone) {
        analyze_reference(analyzer, ast);

        return;
    }

    infer_negated_integer_literal_type(ast);
    analyze_node(analyzer, ast->prefix.expr);

    if (ast->prefix.operator.type == TokenKeywordNot && get_node_data_type(ast->prefix.expr) != TypeBool) {
        semantic_error("Logical not requires a bool operand", ast->prefix.operator);
    }

    DataType type = get_node_data_type(ast->prefix.expr);

    if (type == TypeString) {
        semantic_error("Invalid prefix operator for string operands", ast->prefix.operator);
    }

    if (type == TypeChar) {
        semantic_error("Invalid prefix operator for char operands", ast->prefix.operator);
    }

    bool numeric = is_signed_integer_type(type) || is_float_type(type);

    if (ast->prefix.operator.type == TokenMinus && !numeric) {
        semantic_error("Negation requires a signed numeric operand", ast->prefix.operator);
    }
}

static void analyze_resolved_builtin_call(Analyzer* analyzer, ASTNode* ast)
{
    analyze_expression_nodes(analyzer, &ast->builtin_call.args);

    if (ast->builtin_call.builtin->param_count >= 0) {
        return;
    }

    Token token = ast->builtin_call.token;

    validate_variadic_arguments(&ast->builtin_call.args, ast->builtin_call.builtin, token);
}

static void analyze_node(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case ASTAssignment:
            analyze_assignment(analyzer, ast);
            break;
        case ASTBinary:
            analyze_binary(analyzer, ast);
            break;
        case ASTBuiltinCall:
            analyze_resolved_builtin_call(analyzer, ast);
            break;
        case ASTWhile:
            analyze_while(analyzer, ast);
            break;
        case ASTConditional:
            analyze_conditional(analyzer, ast);
            break;
        case ASTConversion:
            analyze_node(analyzer, ast->conversion.expr);
            break;
        case ASTFunctionCall:
            analyze_function_call(analyzer, ast);
            break;
        case ASTFunctionDefinition:
            analyze_function(analyzer, ast);
            break;
        case ASTMatch:
            analyze_match(analyzer, ast);
            break;
        case ASTParameter:
            analyze_parameter(analyzer, ast);
            break;
        case ASTPrefix:
            analyze_prefix(analyzer, ast);
            break;
        case ASTReturn:
            analyze_return(analyzer, ast);
            break;
        case ASTVariableDefinition:
            analyze_variable_definition(analyzer, ast);
            break;
        case ASTVariable:
            analyze_variable(analyzer, ast);
            break;
        default:
            break;
    }
}

void init_analyzer(Analyzer* analyzer, ASTNode* ast)
{
    analyzer->top_level = ast;
    analyzer->function = NULL;
    analyzer->current_scope = ast->compound.scope;
    analyzer->constant_scope = NULL;
    analyzer->current_nodes = NULL;
    analyzer->next_node = 0;
    analyzer->reference_lifetimes = NULL;
    analyzer->function_path = NULL;
    analyzer->constant_functions = NULL;
    analyzer->loop_path = NULL;
}

bool analyze(Analyzer* analyzer, size_t start)
{
    analyzer->current_scope = analyzer->top_level->compound.scope;
    analyze_nodes(analyzer, &analyzer->top_level->compound.statements, start);
    validate_constant_expressions(analyzer->top_level, start);

    return true;
}

static void clear_analysis_vector(Vector* nodes, Vector* functions)
{
    for (size_t i = 0; i < nodes->count; i++) {
        clear_node_analysis(nodes->data[i], functions);
    }
}

static void clear_variable_analysis(ASTNode* ast, Vector* functions)
{
    ast->variable_definition.scope = NULL;
    ast->variable_definition.initialized = false;
    ast->variable_definition.moved = false;
    ast->variable_definition.shared_access_count = 0;
    ast->variable_definition.exclusive_access_active = false;
    ast->variable_definition.reference_access_active = false;
    ast->variable_definition.reference_origins.count = 0;
    ast->variable_definition.read_count = 0;
    ast->variable_definition.discarded_read_count = 0;
    ast->variable_definition.discarded_reads_analyzed = false;
    ast->variable_definition.address_taken = false;

    clear_node_analysis(ast->variable_definition.expr, functions);
}

static void clear_function_analysis(ASTNode* ast, Vector* functions)
{
    ast->function_definition.scope = NULL;
    ast->function_definition.return_reference_origin = NULL;
    ast->function_definition.call_count = 0;

    push_vector_item(functions, ast);
    clear_analysis_vector(&ast->function_definition.params, functions);
    clear_node_analysis(ast->function_definition.body, functions);
}

static void clear_match_arm_analysis(MatchArm* arm, Vector* functions)
{
    clear_node_analysis(arm->pattern, functions);
    clear_node_analysis(arm->binding, functions);
    clear_node_analysis(arm->guard, functions);
    clear_node_analysis(arm->branch, functions);
}

static void clear_match_analysis(ASTNode* ast, Vector* functions)
{
    ast->match.reference_origins.count = 0;

    clear_node_analysis(ast->match.subject, functions);
    clear_node_analysis(ast->match.default_branch, functions);

    for (size_t i = 0; i < ast->match.arms.count; i++) {
        clear_match_arm_analysis(ast->match.arms.data[i], functions);
    }
}

static void clear_node_analysis(ASTNode* ast, Vector* functions)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case ASTCompound:
            free_scope(ast->compound.scope);
            ast->compound.scope = NULL;
            clear_analysis_vector(&ast->compound.statements, functions);
            break;
        case ASTVariableDefinition:
            clear_variable_analysis(ast, functions);
            break;
        case ASTFunctionDefinition:
            clear_function_analysis(ast, functions);
            break;
        case ASTParameter:
            ast->parameter.scope = NULL;
            ast->parameter.shared_access_count = 0;
            ast->parameter.exclusive_access_active = false;
            ast->parameter.moved = false;
            ast->parameter.reference_origins.count = 0;
            clear_node_analysis(ast->parameter.default_value, functions);
            break;
        case ASTVariable:
            ast->variable.scope = NULL;
            ast->variable.symbol = NULL;
            break;
        case ASTAssignment:
            ast->assignment.scope = NULL;
            ast->assignment.symbol = NULL;
            ast->assignment.initializes_binding = false;
            clear_node_analysis(ast->assignment.expr, functions);
            break;
        case ASTFunctionCall:
            ast->function_call.scope = NULL;
            ast->function_call.symbol = NULL;
            ast->function_call.reference_origins.count = 0;
            clear_analysis_vector(&ast->function_call.args, functions);
            break;
        case ASTBuiltinCall:
            clear_analysis_vector(&ast->builtin_call.args, functions);
            break;
        case ASTConditional:
            ast->conditional.reference_origins.count = 0;
            clear_node_analysis(ast->conditional.condition, functions);
            clear_node_analysis(ast->conditional.then_branch, functions);
            clear_node_analysis(ast->conditional.else_branch, functions);
            break;
        case ASTMatch:
            clear_match_analysis(ast, functions);
            break;
        case ASTWhile:
            clear_node_analysis(ast->while_statement.condition, functions);
            clear_node_analysis(ast->while_statement.body, functions);
            break;
        case ASTBinary:
            clear_node_analysis(ast->binary.left_expr, functions);
            clear_node_analysis(ast->binary.right_expr, functions);
            break;
        case ASTPrefix:
            ast->prefix.reference_origins.count = 0;
            clear_node_analysis(ast->prefix.expr, functions);
            break;
        case ASTConversion:
            clear_node_analysis(ast->conversion.expr, functions);
            break;
        case ASTReturn:
            clear_node_analysis(ast->return_statement.expr, functions);
            break;
        default:
            break;
    }
}

void rebuild_analyzer(Analyzer* analyzer, Vector* functions)
{
    ASTNode* ast = analyzer->top_level;

    init_vector(functions);
    clear_node_analysis(ast, functions);

    ast->compound.scope = create_scope(NULL);

    init_analyzer(analyzer, ast);
    analyze(analyzer, 0);
}
