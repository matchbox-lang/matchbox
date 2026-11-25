#include "compiler/analyzer/recovery.h"
#include "common/vector.h"
#include "compiler/ast/generic.h"
#include "compiler/scope.h"
#include <stdint.h>

static void clear_node_analysis(ASTNode* ast, Vector* functions);

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
    ast->variable_definition.possibly_initialized = false;
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

static void retain_specialization(ASTNode* ast, Vector* nodes, size_t* count)
{
    if (!ast->function_definition.generics->analyzed) {
        free_ast_node(ast);

        return;
    }

    nodes->data[*count] = ast;
    (*count)++;
}

static void discard_incomplete_specializations(ASTNode* ast)
{
    if (!is_generic_function(ast)) {
        return;
    }

    Vector* nodes = &ast->function_definition.generics->specializations->compound.statements;
    size_t count = 0;

    for (size_t i = 0; i < nodes->count; i++) {
        retain_specialization(nodes->data[i], nodes, &count);
    }

    nodes->count = count;
}

static void clear_generic_analysis(ASTNode* ast, Vector* functions)
{
    GenericFunction* generics = ast->function_definition.generics;

    if (!generics) {
        return;
    }

    generics->analyzed = false;

    clear_node_analysis(generics->specializations, functions);
}

static void clear_function_analysis(ASTNode* ast, Vector* functions)
{
    discard_incomplete_specializations(ast);
    clear_function_array_effects(ast);

    ast->function_definition.scope = NULL;
    ast->function_definition.next_overload = NULL;
    ast->function_definition.return_reference_origin = NULL;
    ast->function_definition.call_count = 0;

    push_vector_item(functions, ast);
    clear_analysis_vector(&ast->function_definition.params, functions);
    clear_node_analysis(ast->function_definition.body, functions);
    clear_generic_analysis(ast, functions);
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
        case ASTSlice:
            ast->slice.reference_origins.count = 0;
            clear_node_analysis(ast->slice.expr, functions);
            clear_node_analysis(ast->slice.start, functions);
            clear_node_analysis(ast->slice.end, functions);
            break;
        case ASTArray:
            clear_analysis_vector(&ast->array.elements, functions);
            break;
        case ASTIndex:
        case ASTIndexAssignment:
            clear_node_analysis(ast->index.array, functions);
            clear_node_analysis(ast->index.index, functions);
            clear_node_analysis(ast->index.value, functions);
            break;
        case ASTCompound:
            free_scope(ast->compound.scope);
            ast->compound.scope = NULL;
            clear_analysis_vector(&ast->compound.statements, functions);
            break;
        case ASTVariableDefinition:
            clear_variable_analysis(ast, functions);
            break;
        case ASTTestBlock:
            clear_node_analysis(ast->test_block.function, functions);
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
        case ASTFor:
            clear_node_analysis(ast->for_statement.array, functions);
            clear_node_analysis(ast->for_statement.end, functions);
            clear_node_analysis(ast->for_statement.index, functions);
            clear_node_analysis(ast->for_statement.value, functions);
            clear_node_analysis(ast->for_statement.body, functions);
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

static void analyze_submissions(Analyzer* analyzer, const Vector* submission_boundaries)
{
    Vector* nodes = &analyzer->top_level->compound.statements;
    size_t count = nodes->count;
    size_t start = 0;

    for (size_t i = 0; i < submission_boundaries->count; i++) {
        nodes->count = (size_t)(uintptr_t)submission_boundaries->data[i];

        analyze(analyzer, start);

        start = nodes->count;
    }

    nodes->count = count;

    analyze(analyzer, start);
}

void rebuild_analyzer(Analyzer* analyzer, const Vector* submission_boundaries, Vector* functions)
{
    ASTNode* ast = analyzer->top_level;
    Vector array_types = analyzer->array_types;

    init_vector(functions);
    clear_node_analysis(ast, functions);

    ast->compound.scope = create_scope(NULL);

    init_analyzer(analyzer, ast);
    analyzer->array_types = array_types;

    analyze_submissions(analyzer, submission_boundaries);
}
