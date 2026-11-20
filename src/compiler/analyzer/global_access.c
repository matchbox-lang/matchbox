#include "compiler/analyzer/global_access.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/analyzer/builtin.h"
#include "compiler/analyzer/function.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/variable.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"

static void validate_node_global_access(Analyzer* analyzer, ASTNode* ast);

void validate_global_binding_access(Analyzer* analyzer, ASTNode* origin, ReferenceType type, Token token)
{
    validate_constant_mutation(origin, type, token);

    if (!origin || !is_top_level_scope(get_scope(origin))) {
        return;
    }

    validate_constant_binding(analyzer, origin, token);

    Type value_type = get_node_data_type(origin);
    bool moved = is_moved(origin);

    if (is_array_type(value_type.kind) && moved) {
        semantic_error("Function cannot access a moved array", token);
    }

    if (type == RefShared && has_exclusive_access(origin)) {
        binding_access_error("Function requires shared access to binding ", token, " while exclusive access is active");
    }

    if (type == RefExclusive
        && (has_exclusive_access(origin) || *get_shared_access_count(origin))) {
        binding_access_error("Function requires exclusive access to binding ", token, " while another access is active");
    }
}

static bool is_function_in_path(FunctionPath* path, ASTNode* function)
{
    if (!path || path->function == function) {
        return path != NULL;
    }

    return is_function_in_path(path->previous, function);
}

static void validate_global_access_in_nodes(Analyzer* analyzer, Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        validate_node_global_access(analyzer, node);
    }
}

static void validate_function_array_moves(Analyzer* analyzer, ASTNode* function)
{
    Vector* moves = &function->function_definition.array_moves;

    for (size_t i = 0; i < moves->count; i++) {
        validate_global_binding_access(analyzer, moves->data[i], RefExclusive, function->function_definition.token);
    }
}

static void apply_function_array_moves(ASTNode* function)
{
    Vector* moves = &function->function_definition.array_moves;

    for (size_t i = 0; i < moves->count; i++) {
        set_moved_state(moves->data[i], true);
    }
}

bool contains_binding(const Vector* bindings, const ASTNode* symbol)
{
    size_t position = 0;

    while (position < bindings->count && bindings->data[position] != symbol) {
        position++;
    }

    return position < bindings->count;
}

static void validate_function_array_initialization(ASTNode* function, ASTNode* symbol, bool recursive)
{
    bool initialized = symbol->variable_definition.possibly_initialized;

    if (symbol->variable_definition.fixed && (recursive || initialized)) {
        semantic_error("Function may reinitialize a fixed array binding", function->function_definition.token);
    }
}

static void validate_function_array_initializations(ASTNode* function, bool recursive)
{
    Vector* bindings = &function->function_definition.array_possible_initializations;

    for (size_t i = 0; i < bindings->count; i++) {
        validate_function_array_initialization(function, bindings->data[i], recursive);
    }
}

static void apply_function_possible_initializations(ASTNode* function)
{
    Vector* bindings = &function->function_definition.array_possible_initializations;

    for (size_t i = 0; i < bindings->count; i++) {
        ASTNode* symbol = bindings->data[i];
        symbol->variable_definition.possibly_initialized = true;
    }
}

static void apply_function_array_initializations(ASTNode* function)
{
    Vector* bindings = &function->function_definition.array_initializations;

    apply_function_possible_initializations(function);

    for (size_t i = 0; i < bindings->count; i++) {
        initialize_variable(bindings->data[i]);
    }
}

void validate_function_global_access(Analyzer* analyzer, ASTNode* function, bool moves_arrays)
{
    if (analyzer->constant_scope && is_function_in_path(analyzer->constant_functions, function)) {
        semantic_token_error("Cannot evaluate incomplete function ", function->function_definition.token);
    }

    bool recursive = is_function_in_path(analyzer->function_path, function);

    validate_function_array_initializations(function, recursive);

    if (recursive) {
        return;
    }

    FunctionPath path = {function, analyzer->function_path};
    analyzer->function_path = &path;

    validate_function_array_moves(analyzer, function);
    validate_global_access_in_nodes(analyzer, &function->function_definition.body->compound.statements);

    if (moves_arrays) {
        apply_function_array_moves(function);
        apply_function_array_initializations(function);
    }

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
}

static void validate_prefix_global_access(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* origin = get_reference_origin_from_expression(ast);
    ReferenceType type = get_reference_type(ast);

    if (type != RefNone) {
        validate_constant_reference(analyzer, origin, ast->prefix.operator);
    }

    validate_global_binding_access(analyzer, origin, type, ast->prefix.operator);
    validate_node_global_access(analyzer, ast->prefix.expr);
}

static void validate_index_global_access(Analyzer* analyzer, ASTNode* ast)
{
    validate_node_global_access(analyzer, ast->index.array);
    validate_node_global_access(analyzer, ast->index.index);

    if (ast->type != ASTIndexAssignment) {
        return;
    }

    ASTNode* owner = ast->index.array;

    while (owner->type == ASTIndex) {
        owner = owner->index.array;
    }

    if (owner->type == ASTVariable) {
        ASTNode* origin = get_reference_origin_or_self(owner->variable.symbol);

        validate_global_binding_access(analyzer, origin, RefExclusive, ast->index.token);
    }

    validate_node_global_access(analyzer, ast->index.value);
}

static void validate_node_global_access(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case ASTArray:
            validate_global_access_in_nodes(analyzer, &ast->array.elements);
            break;
        case ASTIndex:
        case ASTIndexAssignment:
            validate_index_global_access(analyzer, ast);
            break;
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
            validate_constant_builtin(analyzer, ast->builtin_call.builtin, ast->builtin_call.token);
            validate_global_access_in_nodes(analyzer, &ast->builtin_call.args);
            break;
        case ASTFor:
            validate_node_global_access(analyzer, ast->for_statement.array);
            validate_branch_global_access(analyzer, ast->for_statement.body);
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
            validate_function_global_access(analyzer, ast->function_call.symbol, false);
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
