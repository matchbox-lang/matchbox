#include "compiler/generator/control_flow.h"
#include "common/vector.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/ast.h"
#include <stddef.h>

Operand compile_logical_binary(Compiler* compiler, ASTNode* ast)
{
    Operand result = compile_expression(compiler, ast->binary.left_expr, false);
    Opcode opcode = OpBz;

    result = materialize_operand(compiler, result);

    if (ast->binary.operator.type == TokenKeywordOr) {
        opcode = OpBnz;
    }

    size_t end_jump = emit_jump(compiler, opcode, result.reg);
    Operand right = compile_expression(compiler, ast->binary.right_expr, false);

    emit_operand_mov(compiler, result.reg, right);
    release_operand(compiler, right);
    patch_jump(compiler, end_jump);

    return result;
}

static void store_conditional_value(Compiler* compiler, Type type, int destination, Operand value)
{
    if (type.kind == TypeVoid) {
        return;
    }

    if (retarget_temporary(compiler, value, destination)) {
        return;
    }

    emit_operand_mov(compiler, destination, value);
}

Operand compile_conditional_branch(Compiler* compiler, ASTNode* branch, Type type, int destination, bool reference)
{
    if (branch->type == ASTConditional) {
        Operand value = compile_converted_expression(compiler, branch, type, reference);

        store_conditional_value(compiler, type, destination, value);

        return value;
    }

    size_t count = count_vector(&branch->compound.statements);

    for (size_t i = 0; i < count; i++) {
        bool discard = type.kind == TypeVoid || i + 1 < count;
        ASTNode* statement = get_vector_at(&branch->compound.statements, i);

        if (discard) {
            compile_statement(compiler, statement, true);

            continue;
        }

        Operand value = compile_converted_expression(compiler, statement, type, reference);

        store_conditional_value(compiler, type, destination, value);

        return value;
    }

    return no_operand();
}

Operand allocate_branch_result(Compiler* compiler, Type type)
{
    if (type.kind == TypeVoid) {
        return no_operand();
    }

    size_t slots = get_type_slot_count(type.kind);
    int reg = allocate_registers(compiler, slots);
    Operand result = {reg, true, slots};
    
    compiler->local_position_offset += slots;

    return result;
}

static void compile_scoped_conditional_branch(
    Compiler* compiler, ASTNode* branch, Type type, int destination, bool reference)
{
    if (!branch) {
        return;
    }

    int register_count = compiler->register_count;

    compile_conditional_branch(compiler, branch, type, destination, reference);

    compiler->register_count = register_count;
}

static void compile_conditional_branches(
    Compiler* compiler, ASTNode* ast, int destination, bool reference, size_t else_jump)
{
    ASTNode* condition = ast->conditional.condition;
    Type type = get_node_data_type(ast);

    if (condition->type == ASTBoolean) {
        ASTNode* branch = condition->boolean_literal.value ? ast->conditional.then_branch : ast->conditional.else_branch;

        compile_scoped_conditional_branch(compiler, branch, type, destination, reference);

        return;
    }

    compile_scoped_conditional_branch(compiler, ast->conditional.then_branch, type, destination, reference);

    if (!ast->conditional.else_branch) {
        patch_jump(compiler, else_jump);

        return;
    }

    size_t end_jump = emit_jump(compiler, OpJmp, 0);

    patch_jump(compiler, else_jump);
    compile_scoped_conditional_branch(compiler, ast->conditional.else_branch, type, destination, reference);
    patch_jump(compiler, end_jump);
}

Operand compile_conditional(Compiler* compiler, ASTNode* ast, bool reference)
{
    int previous_local_position_offset = compiler->local_position_offset;
    ASTNode* condition_node = ast->conditional.condition;
    size_t else_jump = 0;

    if (condition_node->type != ASTBoolean) {
        Operand condition = compile_expression(compiler, condition_node, false);
        else_jump = emit_condition_jump(compiler, condition);
        
        release_operand(compiler, condition);
    }

    Type type = get_node_data_type(ast);
    Operand result = allocate_branch_result(compiler, type);

    compile_conditional_branches(compiler, ast, result.reg, reference, else_jump);

    compiler->local_position_offset = previous_local_position_offset;

    return result;
}

void compile_while(Compiler* compiler, ASTNode* ast)
{
    CodeObject* code = current_code_object(compiler);
    size_t start = count_code_object(code);
    int register_count = compiler->register_count;
    compiler->peephole_barrier = start;

    Operand condition = compile_expression(compiler, ast->while_statement.condition, false);
    size_t exit_jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);
    compile_scoped_conditional_branch(compiler, ast->while_statement.body, (Type){.kind = TypeVoid}, 0, false);
    emit_loop_jump(compiler, start);
    patch_jump(compiler, exit_jump);

    compiler->register_count = register_count;
}
