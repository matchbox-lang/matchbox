#include "compiler/analyzer/control_flow.h"
#include "common/vector.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

ASTNode* get_branch_result(ASTNode* branch)
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

void merge_branch_reference(ASTNode* ast, ASTNode* result, bool first, bool has_expected_type, Token token)
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

Scope* enter_branch_scope(Analyzer* analyzer, ASTNode* branch, ASTNode* binding)
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

Type analyze_branch_contents(Analyzer* analyzer, ASTNode* branch, Type type, bool moves_result)
{
    size_t count = count_vector(&branch->compound.statements);

    if (count) {
        ASTNode* result = get_vector_at(&branch->compound.statements, count - 1);

        apply_literal_type(result, type);
    }

    analyze_expression_nodes(analyzer, &branch->compound.statements);

    if (!count) {
        return (Type){.kind = TypeVoid};
    }

    ASTNode* result = get_vector_at(&branch->compound.statements, count - 1);
    Type result_type = get_node_data_type(result);

    if (moves_result) {
        consume_array(result);
    }

    if (types_equal(result_type, type) || can_implicitly_widen_type(result_type.kind, type.kind)) {
        return type;
    }

    return result_type;
}

static Type analyze_scoped_branch(Analyzer* analyzer, ASTNode* branch, ASTNode* binding, Type type, bool moves_result)
{
    Scope* previous_scope = enter_branch_scope(analyzer, branch, binding);
    Type result_type = analyze_branch_contents(analyzer, branch, type, moves_result);
    analyzer->current_scope = previous_scope;

    return result_type;
}

Type analyze_else_branch(Analyzer* analyzer, ASTNode* branch, Type type, bool moves_result)
{
    if (!branch) {
        return (Type){.kind = TypeVoid};
    }

    if (branch->type == ASTConditional) {
        branch->conditional.type_id = type;
        analyze_node(analyzer, branch);

        return get_node_data_type(branch);
    }

    return analyze_scoped_branch(analyzer, branch, NULL, type, moves_result);
}

void analyze_while(Analyzer* analyzer, ASTNode* ast)
{
    Vector entry_states;
    Vector body_states;
    LoopPath path = {ast, analyzer->loop_path};
    analyzer->loop_path = &path;

    capture_ownership_state(&entry_states, analyzer->current_scope);
    copy_ownership_state(&body_states, &entry_states);
    analyze_node(analyzer, ast->while_statement.condition);

    Type type = get_node_data_type(ast->while_statement.condition);

    if (type.kind != TypeBool) {
        semantic_error("While condition must have type bool", ast->while_statement.token);
    }

    analyze_scoped_branch(analyzer, ast->while_statement.body, NULL, (Type){.kind = TypeVoid}, false);
    validate_loop_ownership(&entry_states, ast->while_statement.token);
    update_ownership_states(&body_states);

    analyzer->loop_path = path.previous;

    merge_ownership_states(&entry_states, &body_states);
    free_ownership_states(&entry_states);
    free_ownership_states(&body_states);
}

void validate_expression_branch(ASTNode* branch, Token token)
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

void analyze_conditional(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuation_nodes = analyzer->current_nodes;
    size_t continuation_start = analyzer->next_node;

    validate_conditional_expression(ast);
    analyze_node(analyzer, ast->conditional.condition);

    if (get_node_data_type(ast->conditional.condition).kind != TypeBool) {
        semantic_error("Condition must have type bool", ast->conditional.token);
    }

    Vector then_states;
    Vector else_states;

    capture_ownership_state(&then_states, analyzer->current_scope);
    copy_ownership_state(&else_states, &then_states);

    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;
    release_references_dead_on_branch(analyzer, &then_states, ast->conditional.then_branch, NULL);

    Type type = ast->conditional.type_id;
    Type then_type = analyze_scoped_branch(
        analyzer, ast->conditional.then_branch, NULL, type, ast->conditional.expression);

    update_ownership_states(&then_states);

    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;

    Type else_type;

    analyze_ownership_branch(analyzer, &else_states, ast->conditional.else_branch, type, &else_type,
        ast->conditional.expression);
    merge_ownership_states(&then_states, &else_states);
    free_ownership_states(&then_states);
    free_ownership_states(&else_states);

    if (!ast->conditional.expression) {
        ast->conditional.type_id = (Type){.kind = TypeVoid};

        return;
    }

    if (!ast->conditional.else_branch) {
        semantic_token_error("If expression requires an else branch ", ast->conditional.token);
    }

    if (then_type.kind == TypeVoid || else_type.kind == TypeVoid) {
        semantic_token_error("Conditional expression branch must produce a value ", ast->conditional.token);
    }

    if (!types_equal(then_type, else_type)) {
        semantic_token_error("Conditional expression branches have incompatible types ", ast->conditional.token);
    }

    bool has_expected_type = ast->conditional.type_id.kind != TypeUnknown
        && ast->conditional.type_id.kind != TypeVoid;

    ASTNode* then_result = get_branch_result(ast->conditional.then_branch);
    ASTNode* else_result = get_branch_result(ast->conditional.else_branch);
    
    merge_branch_reference(ast, then_result, true, has_expected_type, ast->conditional.token);
    merge_branch_reference(ast, else_result, false, has_expected_type, ast->conditional.token);

    ast->conditional.type_id = then_type;
}
