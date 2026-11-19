#include "compiler/generator/match.h"
#include "common/math.h"
#include "common/vector.h"
#include "compiler/generator/control_flow.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/variable.h"
#include "compiler/ast.h"
#include "runtime/code_object.h"
#include <stddef.h>
#include <stdint.h>

typedef enum LiteralMatchArmResult
{
    LiteralMatchArmUnavailable,
    LiteralMatchArmSkipped,
    LiteralMatchArmSelected
} LiteralMatchArmResult;

static Operand compile_match_subject(Compiler* compiler, ASTNode* subject)
{
    Type type = get_node_data_type(subject);
    Operand value = compile_comparison_operand(compiler, subject, type);
    value.temporary = false;

    return value;
}

static bool is_match_alternative(ASTNode* pattern)
{
    return pattern->type == ASTBinary && pattern->binary.operator.type == TokenPipe;
}

static Operand compile_subject_match_condition(Compiler* compiler, ASTNode* pattern, Operand subject)
{
    if (!is_match_alternative(pattern)) {
        Type type = get_node_data_type(pattern);
        Operand value = compile_comparison_operand(compiler, pattern, type);

        return emit_numeric_comparison(compiler, TokenEqualEqual, type, subject, value);
    }

    Operand left = compile_subject_match_condition(compiler, pattern->binary.left_expr, subject);
    Operand right = compile_subject_match_condition(compiler, pattern->binary.right_expr, subject);

    return emit_comparison(compiler, OpOr, left, right);
}

static Operand compile_match_condition(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject)
{
    if (!ast->match.subject) {
        return compile_expression(compiler, arm->pattern, false);
    }

    return compile_subject_match_condition(compiler, arm->pattern, subject);
}

static void patch_match_jumps(Compiler* compiler, Vector* jumps)
{
    size_t count = count_vector(jumps);

    for (size_t i = 0; i < count; i++) {
        void* jump = get_vector_at(jumps, i);
        patch_jump(compiler, (size_t)(uintptr_t)jump);
    }
}

static void mark_match_join(Compiler* compiler, Vector* jumps)
{
    if (!count_vector(jumps)) {
        return;
    }

    patch_match_jumps(compiler, jumps);
}

static bool can_use_direct_match_branch(MatchArm* arm)
{
    size_t count = count_vector(&arm->branch->compound.statements);
    if (count != 1) {
        return false;
    }

    ASTNode* statement = get_vector_at(&arm->branch->compound.statements, 0);

    return is_simple_match_value(statement);
}

static bool is_literal_match_pattern(ASTNode* pattern)
{
    if (pattern->type == ASTInteger || pattern->type == ASTBoolean) {
        return true;
    }

    if (!is_match_alternative(pattern)) {
        return false;
    }

    return is_literal_match_pattern(pattern->binary.left_expr) && is_literal_match_pattern(pattern->binary.right_expr);
}

static size_t compile_match_alternative_jumps(
    Compiler* compiler, ASTNode* pattern, Operand subject, bool last, Vector* matches)
{
    if (is_match_alternative(pattern)) {
        compile_match_alternative_jumps(compiler, pattern->binary.left_expr, subject, false, matches);

        return compile_match_alternative_jumps(compiler, pattern->binary.right_expr, subject, last, matches);
    }

    Opcode opcode = OpNe;

    if (last) {
        opcode = OpEq;
    }

    Operand value = compile_expression(compiler, pattern, false);
    TokenType operator_type = TokenNotEqual;

    if (opcode == OpEq) {
        operator_type = TokenEqualEqual;
    }

    Type type = get_node_data_type(pattern);
    Operand condition = emit_numeric_comparison(compiler, operator_type, type, subject, value);
    size_t jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);

    if (!last) {
        push_vector_item(matches, (void*)(uintptr_t)jump);
    }

    return jump;
}

static size_t compile_match_alternative_condition(Compiler* compiler, ASTNode* pattern, Operand subject)
{
    Vector matches;

    init_vector(&matches);

    size_t next_jump = compile_match_alternative_jumps(compiler, pattern, subject, true, &matches);

    patch_match_jumps(compiler, &matches);
    free_vector(&matches);

    return next_jump;
}

static bool get_literal_match_value(ASTNode* ast, uint64_t* value)
{
    if (ast->type == ASTBoolean) {
        *value = ast->boolean_literal.value;

        return true;
    }

    if (ast->type == ASTCharacter) {
        *value = ast->character_literal.value;

        return true;
    }

    if (ast->type != ASTInteger) {
        return false;
    }

    *value = ast->integer_literal.value;

    if (ast->integer_literal.type_id.kind == TypeI32) {
        *value = (uint64_t)(int64_t)(int32_t)*value;
    }

    return true;
}

static size_t compile_match_arm_condition(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject)
{
    if (!arm->pattern) {
        return 0;
    }

    Type subject_type = get_node_data_type(ast->match.subject);
    size_t subject_size = get_integer_type_size(subject_type.kind);

    if (ast->match.subject && subject.slots == 1 && is_match_alternative(arm->pattern)
        && is_literal_match_pattern(arm->pattern) && subject_size != 8) {
        return compile_match_alternative_condition(compiler, arm->pattern, subject);
    }

    Type pattern_type = get_node_data_type(arm->pattern);
    bool numeric_compare = get_integer_type_size(pattern_type.kind) == 8 || is_float_type(pattern_type.kind);

    if (!ast->match.subject || is_match_alternative(arm->pattern) || !can_use_direct_match_branch(arm) || numeric_compare) {
        Operand condition = compile_match_condition(compiler, ast, arm, subject);
        size_t next_jump = emit_condition_jump(compiler, condition);
        release_operand(compiler, condition);

        return next_jump;
    }

    uint64_t pattern_value;

    if (subject.slots == 1 && get_literal_match_value(arm->pattern, &pattern_value) && pattern_value == 0) {
        return emit_jump(compiler, OpBnz, subject.reg);
    }

    Operand pattern = compile_expression(compiler, arm->pattern, false);
    Operand condition = emit_immediate_comparison(compiler, OpEq, subject, pattern);

    if (condition.reg >= 0) {
        size_t next_jump = emit_condition_jump(compiler, condition);

        release_operand(compiler, condition);

        return next_jump;
    }

    CodeObject* code = get_current_code_object(compiler);
    size_t next_jump = count_code_object(code);

    emit_instruction(compiler, OpBne, subject.reg, pattern.reg, 0);
    release_operand(compiler, pattern);

    return next_jump;
}

static void store_match_binding(Compiler* compiler, ASTNode* binding, Operand subject)
{
    if (!binding) {
        return;
    }

    binding->variable_definition.register_position = compiler->frame_base_count + compiler->local_position_offset
        + binding->variable_definition.position;
    int position = get_local_position(compiler, binding);

    emit_operand_mov(compiler, position, subject);

    Type binding_type = get_node_data_type(binding);
    int binding_register_count = position + get_type_slot_count(binding_type.kind);

    compiler->register_count = MAX(compiler->register_count, binding_register_count);
}

static size_t compile_match_guard(Compiler* compiler, MatchArm* arm)
{
    if (!arm->guard) {
        return 0;
    }

    Operand condition = compile_expression(compiler, arm->guard, false);
    size_t next_jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);

    return next_jump;
}

static size_t compile_match_arm(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject, int destination,
    bool needs_end_jump, bool reference)
{
    int branch_register_count = compiler->register_count;
    size_t next_jump = compile_match_arm_condition(compiler, ast, arm, subject);
    Type type = get_node_data_type(ast);

    store_match_binding(compiler, arm->binding, subject);

    size_t guard_jump = compile_match_guard(compiler, arm);

    compile_conditional_branch(compiler, arm->branch, type, destination, reference);
    compiler->register_count = branch_register_count;

    size_t end_jump = SIZE_MAX;

    if (needs_end_jump) {
        end_jump = emit_jump(compiler, OpJmp, 0);
    }

    if (arm->pattern) {
        patch_jump(compiler, next_jump);
    }

    if (arm->guard) {
        patch_jump(compiler, guard_jump);
    }

    return end_jump;
}

static void compile_next_match_arm(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject, int destination,
    bool has_following_arm, Vector* end_jumps, bool reference)
{
    bool unconditional = !arm->pattern && !arm->guard;
    bool needs_end_jump = !unconditional && has_following_arm;
    size_t end_jump = compile_match_arm(compiler, ast, arm, subject, destination, needs_end_jump, reference);

    if (end_jump != SIZE_MAX) {
        push_vector_item(end_jumps, (void*)(uintptr_t)end_jump);
    }
}

static void compile_match_arms(
    Compiler* compiler, ASTNode* ast, Operand subject, int destination, Vector* end_jumps, bool reference)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        bool has_following_arm = i + 1 < count;

        compile_next_match_arm(compiler, ast, arm, subject, destination, has_following_arm, end_jumps, reference);
    }
}

static Operand match_result_operand(Type type, int destination)
{
    if (type.kind == TypeVoid) {
        return no_operand();
    }

    if (get_type_slot_count(type.kind) == 2) {
        return make_wide_operand(destination, true);
    }

    return make_operand(destination, true);
}

static bool get_literal_match_subject(ASTNode* ast, uint64_t* value)
{
    if (!ast->match.subject) {
        *value = true;

        return true;
    }

    return get_literal_match_value(ast->match.subject, value);
}

static bool evaluate_literal_match_pattern(ASTNode* pattern, uint64_t subject_value, bool* matches)
{
    uint64_t pattern_value;

    if (get_literal_match_value(pattern, &pattern_value)) {
        *matches = subject_value == pattern_value;

        return true;
    }

    if (!is_match_alternative(pattern)) {
        return false;
    }

    bool left;
    bool right;

    if (!evaluate_literal_match_pattern(pattern->binary.left_expr, subject_value, &left)
        || !evaluate_literal_match_pattern(pattern->binary.right_expr, subject_value, &right)) {
        return false;
    }

    *matches = left || right;

    return true;
}

static LiteralMatchArmResult select_literal_match_arm(
    MatchArm* arm, uint64_t subject_value, ASTNode** branch, ASTNode** binding)
{
    if (arm->guard) {
        return LiteralMatchArmUnavailable;
    }

    if (!arm->pattern) {
        *branch = arm->branch;
        *binding = arm->binding;

        return LiteralMatchArmSelected;
    }

    bool matches;

    if (!evaluate_literal_match_pattern(arm->pattern, subject_value, &matches)) {
        return LiteralMatchArmUnavailable;
    }

    if (!matches) {
        return LiteralMatchArmSkipped;
    }

    *branch = arm->branch;
    *binding = NULL;

    return LiteralMatchArmSelected;
}

static bool find_literal_match_branch(ASTNode* ast, ASTNode** branch, ASTNode** binding)
{
    uint64_t subject_value;
    if (!get_literal_match_subject(ast, &subject_value)) {
        return false;
    }

    size_t count = count_vector(&ast->match.arms);
    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        LiteralMatchArmResult result = select_literal_match_arm(arm, subject_value, branch, binding);

        switch (result) {
            case LiteralMatchArmUnavailable:
                return false;
            case LiteralMatchArmSelected:
                return true;
            case LiteralMatchArmSkipped:
                break;
        }
    }

    *branch = NULL;
    *binding = NULL;

    return true;
}

static bool compile_literal_match(Compiler* compiler, ASTNode* ast, Type type, int destination, bool reference)
{
    ASTNode* branch;
    ASTNode* binding;

    if (!find_literal_match_branch(ast, &branch, &binding)) {
        return false;
    }

    if (!branch) {
        return true;
    }

    if (binding) {
        Operand subject = compile_match_subject(compiler, ast->match.subject);
        store_match_binding(compiler, binding, subject);
    }

    compile_conditional_branch(compiler, branch, type, destination, reference);

    return true;
}

static void restore_match_compiler_state(Compiler* compiler, Type type, int previous_local_position_offset,
    int previous_register_count)
{
    compiler->local_position_offset = previous_local_position_offset;
    compiler->register_count = previous_register_count;

    if (type.kind != TypeVoid) {
        compiler->register_count += get_type_slot_count(type.kind);
    }
}

Operand compile_match(Compiler* compiler, ASTNode* ast, bool reference)
{
    int previous_local_position_offset = compiler->local_position_offset;
    int previous_register_count = compiler->register_count;
    Type type = get_node_data_type(ast);
    Operand result = allocate_branch_result(compiler, type);
    int destination = result.reg;

    if (compile_literal_match(compiler, ast, type, destination, reference)) {
        restore_match_compiler_state(compiler, type, previous_local_position_offset, previous_register_count);

        return match_result_operand(type, destination);
    }

    Operand subject = no_operand();

    if (ast->match.subject) {
        subject = compile_match_subject(compiler, ast->match.subject);
    }

    Vector end_jumps;
    init_vector(&end_jumps);

    compile_match_arms(compiler, ast, subject, destination, &end_jumps, reference);

    mark_match_join(compiler, &end_jumps);
    free_vector(&end_jumps);

    restore_match_compiler_state(compiler, type, previous_local_position_offset, previous_register_count);

    return match_result_operand(type, destination);
}
