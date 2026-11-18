#include "compiler/analyzer/analyzer.h"
#include "common/vector.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/builtin.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/expression.h"
#include "compiler/analyzer/function.h"
#include "compiler/analyzer/lifetime.h"
#include "compiler/analyzer/match.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/variable.h"
#include "compiler/validation.h"

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

void analyze_expression_nodes(Analyzer* analyzer, Vector* nodes)
{
    analyze_nodes(analyzer, nodes, 0);
}

void analyze_node(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case ASTArray:
            analyze_array(analyzer, ast);
            break;
        case ASTIndex:
            analyze_index(analyzer, ast);
            break;
        case ASTIndexAssignment:
            analyze_index_assignment(analyzer, ast);
            break;
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
    init_vector(&analyzer->array_types);

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
