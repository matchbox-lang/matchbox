#include "compiler/analyzer/match.h"
#include "common/vector.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/ast/control_flow.h"
#include "compiler/ast/query.h"
#include "compiler/diagnostics.h"

static void update_boolean_match_coverage(MatchArm* arm, bool* covers_false, bool* covers_true);

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

    return get_node_data_type(ast->match.subject).kind == TypeBool;
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
    return match_has_wildcard(ast) || match_covers_both_booleans(ast);
}

static void merge_match_branch_type(ASTNode* ast, Type branch_type, Type* result_type)
{
    if (branch_type.kind == TypeVoid) {
        semantic_token_error("Match expression arm must produce a value ", ast->match.token);
    }

    if (result_type->kind == TypeUnknown) {
        *result_type = branch_type;

        return;
    }

    if (!types_equal(*result_type, branch_type)) {
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
    Type expected = ast->match.subject ? get_node_data_type(ast->match.subject) : (Type){.kind = TypeBool};

    if (is_integer_type(expected.kind)) {
        apply_integer_literal_type(pattern, expected);
    }

    if (expected.kind == TypeString) {
        semantic_token_error("String match patterns are not supported ", ast->match.token);
    }

    Type pattern_type = get_node_data_type(pattern);

    if (!types_equal(pattern_type, expected)) {
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

static void merge_match_result(Analyzer* analyzer, ASTNode* ast, ASTNode* branch, Type branch_type, Type* result_type)
{
    if (!ast->match.expression) {
        return;
    }

    bool first = result_type->kind == TypeUnknown;
    bool has_expected_type = ast->match.type_id.kind != TypeUnknown && ast->match.type_id.kind != TypeVoid;
    ASTNode* result = get_branch_result(branch);
    
    merge_branch_reference(analyzer, ast, result, first, has_expected_type, ast->match.token);
    merge_match_branch_type(ast, branch_type, result_type);
}

static void analyze_match_guard(Analyzer* analyzer, MatchArm* arm, Vector* fallthrough_states)
{
    if (!arm->guard) {
        return;
    }

    analyze_node(analyzer, arm->guard);

    if (get_node_data_type(arm->guard).kind != TypeBool) {
        semantic_error("Match guard requires a bool operand", arm->guard_token);
    }

    Vector guard_states;

    copy_ownership_states(&guard_states, fallthrough_states);
    update_ownership_states(analyzer, &guard_states);

    if (arm->pattern) {
        merge_ownership_states(analyzer, fallthrough_states, &guard_states);
        update_ownership_states(analyzer, fallthrough_states);
        apply_ownership_states(analyzer, &guard_states);
    } else {
        update_ownership_states(analyzer, fallthrough_states);
    }

    release_references_dead_on_branch(analyzer, &guard_states, arm->branch, arm->binding);
    free_ownership_states(&guard_states);
}

static void analyze_match_arm(Analyzer* analyzer, ASTNode* ast, MatchArm* arm, Vector* continuation_nodes,
    size_t continuation_start, Type* result_type, Vector* fallthrough_states)
{
    restore_match_continuation(analyzer, continuation_nodes, continuation_start);
    analyze_match_pattern(analyzer, ast, arm);

    if (arm->binding) {
        arm->binding->variable_definition.type_id = get_node_data_type(ast->match.subject);
    }

    Scope* previous_scope = enter_branch_scope(analyzer, arm->branch, arm->binding);

    analyze_match_guard(analyzer, arm, fallthrough_states);

    Type branch_type = analyze_branch_contents(analyzer, arm->branch, ast->match.type_id, ast->match.expression);
    analyzer->current_scope = previous_scope;

    merge_match_result(analyzer, ast, arm->branch, branch_type, result_type);
}

static void analyze_match_arm_ownership(Analyzer* analyzer, ASTNode* ast, MatchArm* arm,
    Vector* continuation_nodes, size_t continuation_start, Type* result_type,
    Vector* initial_states, Vector* merged_states, bool* merged_initialized)
{
    Vector states;
    copy_ownership_states(&states, initial_states);
    apply_ownership_states(analyzer, &states);

    if (!arm->guard) {
        release_references_dead_on_branch(analyzer, &states, arm->branch, arm->binding);
    }

    analyze_match_arm(analyzer, ast, arm, continuation_nodes, continuation_start, result_type, initial_states);
    update_ownership_states(analyzer, &states);
    merge_ownership_outcome(analyzer, merged_states, &states, merged_initialized);
    free_ownership_states(&states);
}

static void analyze_match_arms(Analyzer* analyzer, ASTNode* ast, Vector* continuation_nodes, size_t continuation_start,
    Type* result_type, Vector* initial_states, Vector* merged_states, bool* merged_initialized)
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

static void finalize_match_type(ASTNode* ast, Type result_type)
{
    if (!ast->match.expression) {
        ast->match.type_id = (Type){.kind = TypeVoid};

        return;
    }

    if (!match_is_exhaustive(ast)) {
        semantic_token_error("Match expression must be exhaustive ", ast->match.token);
    }

    ast->match.type_id = result_type;
}

static void finalize_match_ownership(Analyzer* analyzer, ASTNode* ast, Vector* initial_states,
    Vector* merged_states, bool* merged_initialized)
{
    if (!match_is_exhaustive(ast)) {
        merge_ownership_outcome(analyzer, merged_states, initial_states, merged_initialized);
    }

    apply_ownership_states(analyzer, merged_states);
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
}

void analyze_match(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuation_nodes = analyzer->current_nodes;
    size_t continuation_start = analyzer->next_node;
    Type result_type = (Type){.kind = TypeUnknown};
    Vector initial_states;
    Vector merged_states;
    bool merged_initialized = false;

    validate_match_expression(ast);
    analyze_match_subject(analyzer, ast);
    capture_ownership_states(analyzer, &initial_states, analyzer->current_scope);
    analyze_match_arms(analyzer, ast, continuation_nodes, continuation_start, &result_type,
        &initial_states, &merged_states, &merged_initialized);
    restore_match_continuation(analyzer, continuation_nodes, continuation_start);
    finalize_match_ownership(analyzer, ast, &initial_states, &merged_states, &merged_initialized);
    finalize_match_type(ast, result_type);
}
