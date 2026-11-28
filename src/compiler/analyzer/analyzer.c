#include "compiler/analyzer/analyzer.h"
#include "common/vector.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/builtin.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/expression.h"
#include "compiler/analyzer/function.h"
#include "compiler/analyzer/index.h"
#include "compiler/analyzer/lifetime.h"
#include "compiler/analyzer/match.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/slice.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/analyzer/tuple_reference.h"
#include "compiler/analyzer/type_alias.h"
#include "compiler/analyzer/validation.h"
#include "compiler/analyzer/variable.h"

void init_analyzer(Analyzer* analyzer, ASTNode* ast)
{
    init_vector(&analyzer->array_types);
    init_vector(&analyzer->tuple_types);
    init_table(&analyzer->tuple_references, 128);

    analyzer->top_level = ast;
    analyzer->tuple_projection = false;
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

static void analyze_pending_nodes(Analyzer* analyzer, Vector* nodes, size_t start, ReferenceLifetimes* lifetimes)
{
    for (size_t i = start; i < nodes->count; i++) {
        ASTNode* ast = get_vector_at(nodes, i);
        analyzer->current_nodes = nodes;
        analyzer->next_node = i + 1;

        analyze_node(analyzer, ast);
        schedule_reference_release(lifetimes, ast, i);
        release_inactive_reference_access(analyzer, &lifetimes->releases[i]);
    }
}

static void analyze_nodes(Analyzer* analyzer, Vector* nodes, size_t start)
{
    size_t count = count_vector(nodes);

    if (start >= count) {
        return;
    }

    register_type_aliases(analyzer, nodes, start);

    ReferenceLifetimes lifetimes;
    ReferenceLifetimes* previous_lifetimes = analyzer->reference_lifetimes;
    Vector* previous_nodes = analyzer->current_nodes;
    size_t previous_next = analyzer->next_node;

    init_reference_lifetimes(&lifetimes, nodes, start);

    analyzer->reference_lifetimes = &lifetimes;

    analyze_pending_nodes(analyzer, nodes, start, &lifetimes);

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
    clear_tuple_references(analyzer, ast);

    switch (ast->type) {
        case ASTTypeAlias:
            resolve_type_alias(analyzer, ast);
            break;
        case ASTArray:
            analyze_array(analyzer, ast);
            break;
        case ASTTuple:
            analyze_tuple(analyzer, ast);
            break;
        case ASTSlice:
            return analyze_slice(analyzer, ast);
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
        case ASTFor:
            analyze_for(analyzer, ast);
            break;
        case ASTBreak:
        case ASTContinue:
            analyze_loop_control(analyzer, ast);
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
        case ASTTestBlock:
            analyze_test_block(analyzer, ast);
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

bool analyze(Analyzer* analyzer, size_t start)
{
    analyzer->current_scope = analyzer->top_level->compound.scope;
    
    analyze_nodes(analyzer, &analyzer->top_level->compound.statements, start);
    validate_constant_expressions(analyzer->top_level, start);
    discard_transient_tuple_references(analyzer);

    return true;
}
