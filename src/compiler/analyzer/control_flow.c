#include "compiler/analyzer/control_flow.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/control_flow.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

void merge_branch_reference(ASTNode* ast, ASTNode* result, bool first, bool has_expected_type, Token token)
{
    ReferenceType type = get_reference_type(result);

    if (first && !has_expected_type) {
        set_expression_reference_type(ast, type);
    } else if (get_reference_type(ast) != type) {
        semantic_token_error("Expression branches have incompatible reference types ", token);
    }

    Vector* origins = get_reference_origins_from_expression(result);
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
    LoopPath path = {ast, analyzer->loop_path, &entry_states};
    analyzer->loop_path = &path;

    capture_ownership_states(&entry_states, analyzer->current_scope);
    copy_ownership_states(&body_states, &entry_states);
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

static void analyze_conditional_condition(Analyzer* analyzer, ASTNode* ast)
{
    analyze_node(analyzer, ast->conditional.condition);

    Type type = get_node_data_type(ast->conditional.condition);

    if (type.kind != TypeBool) {
        semantic_error("Condition must have type bool", ast->conditional.token);
    }
}

static void analyze_conditional_branches(Analyzer* analyzer, ASTNode* ast, Type* then_type, Type* else_type)
{
    Vector* continuation_nodes = analyzer->current_nodes;
    size_t continuation_start = analyzer->next_node;
    Vector then_states;
    Vector else_states;

    analyze_conditional_condition(analyzer, ast);
    capture_ownership_states(&then_states, analyzer->current_scope);
    copy_ownership_states(&else_states, &then_states);

    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;

    release_references_dead_on_branch(analyzer, &then_states, ast->conditional.then_branch, NULL);

    Type type = ast->conditional.type_id;
    *then_type = analyze_scoped_branch(analyzer, ast->conditional.then_branch, NULL, type, ast->conditional.expression);

    update_ownership_states(&then_states);

    analyzer->current_nodes = continuation_nodes;
    analyzer->next_node = continuation_start;

    analyze_ownership_branch(analyzer, &else_states, ast->conditional.else_branch, type, else_type,
        ast->conditional.expression);
    merge_ownership_states(&then_states, &else_states);
    free_ownership_states(&then_states);
    free_ownership_states(&else_states);
}

static void validate_conditional_result(ASTNode* ast, Type then_type, Type else_type)
{
    if (!ast->conditional.else_branch) {
        semantic_token_error("If expression requires an else branch ", ast->conditional.token);
    }

    if (then_type.kind == TypeVoid || else_type.kind == TypeVoid) {
        semantic_token_error("Conditional expression branch must produce a value ", ast->conditional.token);
    }

    if (!types_equal(then_type, else_type)) {
        semantic_token_error("Conditional expression branches have incompatible types ", ast->conditional.token);
    }
}

static void merge_conditional_references(ASTNode* ast)
{
    bool has_expected_type = ast->conditional.type_id.kind != TypeUnknown
        && ast->conditional.type_id.kind != TypeVoid;
    ASTNode* then_result = get_branch_result(ast->conditional.then_branch);
    ASTNode* else_result = get_branch_result(ast->conditional.else_branch);

    merge_branch_reference(ast, then_result, true, has_expected_type, ast->conditional.token);
    merge_branch_reference(ast, else_result, false, has_expected_type, ast->conditional.token);
}

void analyze_conditional(Analyzer* analyzer, ASTNode* ast)
{
    Type then_type;
    Type else_type;

    validate_conditional_expression(ast);
    analyze_conditional_branches(analyzer, ast, &then_type, &else_type);

    if (!ast->conditional.expression) {
        ast->conditional.type_id = (Type){.kind = TypeVoid};

        return;
    }

    validate_conditional_result(ast, then_type, else_type);
    merge_conditional_references(ast);

    ast->conditional.type_id = then_type;
}

static void resolve_for_binding_type(ASTNode* binding, Type type)
{
    Type declared = binding->variable_definition.type_id;

    if (declared.kind == TypeUnknown) {
        binding->variable_definition.type_id = type;
    } else if (!types_equal(type, declared) && !can_implicitly_widen_type(type.kind, declared.kind)) {
        semantic_error("For binding type is incompatible with the element type", binding->variable_definition.token);
    }

    if (binding->variable_definition.reference_type != RefNone) {
        semantic_error("For bindings require value types", binding->variable_definition.token);
    }
}

static void define_for_binding(Analyzer* analyzer, ASTNode* binding, StringView id)
{
    binding->variable_definition.scope = analyzer->current_scope;
    binding->variable_definition.position = get_next_local_position(analyzer->current_scope);

    initialize_variable(binding);
    set_local_variable_symbol(analyzer->current_scope, id, binding);
}

static void analyze_for_binding(Analyzer* analyzer, ASTNode* binding, Type type)
{
    if (!binding) {
        return;
    }

    resolve_dimensions(analyzer, &binding->variable_definition.type_id);

    StringView id = get_string_view(binding->variable_definition.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Variable", binding->variable_definition.token);
    }

    resolve_for_binding_type(binding, type);
    define_for_binding(analyzer, binding, id);
}

static void collect_for_origins(ASTNode* array, Vector* origins)
{
    if (array->type == ASTIndex) {
        collect_for_origins(array->index.array, origins);

        return;
    }

    Vector* references = get_reference_origins_from_expression(array);

    merge_reference_origins(origins, references);

    if (array->type == ASTVariable) {
        add_reference_origin(origins, array->variable.symbol);
    }
}

static void adjust_for_shared_access_counts(Vector* origins, int change)
{
    for (size_t i = 0; i < origins->count; i++) {
        ASTNode* origin = origins->data[i];
        size_t* count = get_shared_access_count(origin);
        *count += change;
    }
}

static Type analyze_for_array(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* array = ast->for_statement.array;

    analyze_node(analyzer, array);

    Type type = get_node_data_type(array);

    if (!is_contiguous_type(type.kind)) {
        semantic_error("For iteration requires an array", ast->for_statement.token);
    }

    return type;
}

static void analyze_for_index(Analyzer* analyzer, ASTNode* index)
{
    if (!index) {
        return;
    }

    Type type = get_node_data_type(index);

    if (type.kind != TypeUnknown && type.kind != TypeUsize) {
        semantic_error("For index binding requires usize", index->variable_definition.token);
    }

    analyze_for_binding(analyzer, index, (Type){.kind = TypeUsize});
}

static void analyze_for_body(Analyzer* analyzer, ASTNode* ast, Type element_type)
{
    ASTNode* body = ast->for_statement.body;
    Scope* previous_scope = enter_branch_scope(analyzer, body, NULL);

    analyze_for_index(analyzer, ast->for_statement.index);
    analyze_for_binding(analyzer, ast->for_statement.value, element_type);
    analyze_branch_contents(analyzer, body, (Type){.kind = TypeVoid}, false);

    analyzer->current_scope = previous_scope;
}

static void analyze_for_ownership(Analyzer* analyzer, ASTNode* ast, Type element_type)
{
    Vector entry_states;
    Vector body_states;
    LoopPath path = {ast, analyzer->loop_path, &entry_states};
    analyzer->loop_path = &path;

    capture_ownership_states(&entry_states, analyzer->current_scope);
    copy_ownership_states(&body_states, &entry_states);
    analyze_for_body(analyzer, ast, element_type);
    validate_loop_ownership(&entry_states, ast->for_statement.token);
    update_ownership_states(&body_states);

    analyzer->loop_path = path.previous;

    merge_ownership_states(&entry_states, &body_states);
    free_ownership_states(&entry_states);
    free_ownership_states(&body_states);
}

static void analyze_range_bound(Analyzer* analyzer, ASTNode* bound, Token token)
{
    Type type = {.kind = TypeUsize};

    apply_literal_type(bound, type);
    analyze_node(analyzer, bound);

    Type actual = get_node_data_type(bound);
    bool compatible = can_implicitly_widen_integer(actual.kind, TypeUsize);

    if (!compatible || get_reference_type(bound) != RefNone) {
        semantic_error("Range bound requires usize", token);
    }
}

void analyze_for(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->for_statement.end) {
        analyze_range_bound(analyzer, ast->for_statement.array, ast->for_statement.token);
        analyze_range_bound(analyzer, ast->for_statement.end, ast->for_statement.token);
        analyze_for_ownership(analyzer, ast, (Type){.kind = TypeUsize});

        return;
    }

    Type type = analyze_for_array(analyzer, ast);
    Vector origins = {0};

    collect_for_origins(ast->for_statement.array, &origins);
    adjust_for_shared_access_counts(&origins, 1);
    analyze_for_ownership(analyzer, ast, type.array->element_type);
    adjust_for_shared_access_counts(&origins, -1);
    free_vector(&origins);
}

void analyze_loop_control(Analyzer* analyzer, ASTNode* ast)
{
    LoopPath* path = analyzer->loop_path;

    if (!path) {
        semantic_error("Loop control requires an enclosing loop", ast->loop_control.token);
    }

    validate_loop_ownership(path->entry_states, ast->loop_control.token);
}
