#include "compiler/generator/variable.h"
#include "common/array_type.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/lexer/token.h"
#include "compiler/ast.h"
#include "compiler/scope.h"
#include <stddef.h>
#include <stdint.h>

static void compile_power_assignment(Compiler* compiler, ASTNode* ast);

int get_local_position(Compiler* compiler, ASTNode* ast)
{
    int position;

    if (is_parameter(ast)) {
        position = ast->parameter.position;
    } else {
        position = ast->variable_definition.register_position;
    }

    size_t slots = get_node_slot_count(ast);
    int extent = position + slots;

    check_register_count(compiler, extent);

    return position;
}

static Operand load_global_with_store_forwarding(Compiler* compiler, size_t position)
{
    PreviousInstruction previous;
    uint16_t stored_position;

    if (!get_previous_instruction(compiler, &previous)) {
        return make_operand(emit_ldg(compiler, position), true);
    }

    if (previous.opcode == OpStg) {
        stored_position = previous.b << 8 | previous.c;
    } else if (previous.opcode == OpStgi) {
        stored_position = previous.c;
    } else {
        return make_operand(emit_ldg(compiler, position), true);
    }

    if (stored_position != position || previous.a > compiler->register_count) {
        return make_operand(emit_ldg(compiler, position), true);
    }

    if (previous.a == compiler->register_count) {
        int reg = allocate_register(compiler);

        return make_operand(reg, true);
    }

    return make_operand(previous.a, false);
}

static Operand load_global_variable(Compiler* compiler, ASTNode* ast)
{
    size_t position = ast->variable_definition.position;
    Type type = get_node_data_type(ast);

    if (type.kind == TypeSlice) {
        int reg = emit_ldg(compiler, position);

        emit_ldg(compiler, position + 1);

        return make_slice_operand(reg, true);
    }

    bool wide = get_node_slot_count(ast) == 2;

    if (wide) {
        int reg = allocate_registers(compiler, 2);
        emit_global_instruction(compiler, OpLdg64, reg, position);

        return make_wide_operand(reg, true);
    }

    return load_global_with_store_forwarding(compiler, position);
}

static Operand load_local_variable(Compiler* compiler, ASTNode* ast)
{
    int position = get_local_position(compiler, ast);

    Type type = get_node_data_type(ast);

    if (type.kind == TypeSlice) {
        return make_slice_operand(position, false);
    }

    return get_node_slot_count(ast) == 2
        ? make_wide_operand(position, false)
        : make_operand(position, false);
}

static Operand load_variable(Compiler* compiler, ASTNode* ast)
{
    if (is_top_level_scope(ast->variable_definition.scope)) {
        return load_global_variable(compiler, ast);
    }

    return load_local_variable(compiler, ast);
}

Operand reference_variable(Compiler* compiler, ASTNode* ast)
{
    if (get_reference_type(ast) != RefNone) {
        return load_variable(compiler, ast);
    }

    int reg = allocate_register(compiler);

    if (is_top_level_scope(get_scope(ast))) {
        size_t position = ast->variable_definition.position;
        emit_global_instruction(compiler, OpRefg, reg, position);

        return make_operand(reg, true);
    }

    emit_instruction(compiler, OpRefl, reg, get_local_position(compiler, ast), 0);

    return make_operand(reg, true);
}

Operand load_variable_value(Compiler* compiler, ASTNode* symbol)
{
    Operand value = load_variable(compiler, symbol);

    Type type = get_node_data_type(symbol);

    if (get_reference_type(symbol) == RefNone || type.kind == TypeSlice) {
        return value;
    }

    size_t slots = get_type_slot_count(type.kind);

    return dereference_operand(compiler, value, slots);
}

static void store_global_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    size_t position = ast->variable_definition.position;

    if (value.pair) {
        emit_global_instruction(compiler, OpStg, value.reg, position);
        emit_global_instruction(compiler, OpStg, value.reg + 1, position + 1);

        return;
    }

    if (value.slots == 2) {
        emit_global_instruction(compiler, OpStg64, value.reg, position);

        return;
    }

    emit_global_instruction(compiler, OpStg, value.reg, position);
}

static void store_local_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = get_local_position(compiler, ast);

    emit_operand_to_register(compiler, position, value);
}

static void store_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (is_top_level_scope(ast->variable_definition.scope)) {
        store_global_variable(compiler, ast, value);
    } else {
        store_local_variable(compiler, ast, value);
    }

    release_operand(compiler, value);
}

bool is_direct_variable(Compiler* compiler, ASTNode* ast)
{
    if (!is_variable(ast)) {
        return false;
    }

    ASTNode* symbol = ast->variable.symbol;

    return !is_top_level_scope(symbol->variable_definition.scope)
        || is_compiling_top_level(compiler);
}

Operand compile_variable(Compiler* compiler, ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (is_variable_definition(symbol) && symbol->variable_definition.constant) {
        Type type = get_node_data_type(symbol);

        return compile_converted_expression(compiler, symbol->variable_definition.expr, type, false);
    }

    return load_variable_value(compiler, symbol);
}

static void store_assignment_value(Compiler* compiler, ASTNode* symbol, Operand value)
{
    if (get_reference_type(symbol) == RefNone) {
        store_variable(compiler, symbol, value);

        return;
    }

    Operand reference = load_variable(compiler, symbol);
    Opcode opcode = value.slots == 2 ? OpStr64 : OpStr;

    emit_instruction(compiler, opcode, value.reg, reference.reg, 0);
    release_operand(compiler, value);
    release_operand(compiler, reference);
}

static void compile_compound_assignment(Compiler* compiler, ASTNode* ast, IntegerOperation operation)
{
    Type type = get_node_data_type(ast->assignment.symbol);

    Operand left = load_variable_value(compiler, ast->assignment.symbol);

    if (!left.temporary && !is_direct_variable(compiler, ast->assignment.expr)) {
        left = materialize_operand(compiler, left);
    }

    Operand right = compile_converted_expression(compiler, ast->assignment.expr, type, false);
    Operand result;

    if (is_float_type(type.kind)) {
        Opcode opcode = get_float_opcode(type, ast->assignment.operator.type);

        result = emit_float_operands(compiler, opcode, type, left, right);
    } else {
        result = emit_binary_operands(compiler, operation, type, left, right);
    }

    store_assignment_value(compiler, ast->assignment.symbol, result);
}

static void compile_simple_assignment(Compiler* compiler, ASTNode* ast)
{
    if (get_reference_type(ast->assignment.symbol) != RefNone) {
        Operand reference = compile_reference_expression(compiler, ast->assignment.expr);
        store_variable(compiler, ast->assignment.symbol, reference);

        return;
    }

    Operand value = compile_converted_expression(
        compiler, ast->assignment.expr, get_node_data_type(ast->assignment.symbol), false);

    store_assignment_value(compiler, ast->assignment.symbol, value);
}

static void compile_assignment_operator(Compiler* compiler, ASTNode* ast)
{
    switch (ast->assignment.operator.type) {
        case TokenPlusEqual:
            return compile_compound_assignment(compiler, ast, IntAdd);
        case TokenMinusEqual:
            return compile_compound_assignment(compiler, ast, IntSub);
        case TokenStarEqual:
            return compile_compound_assignment(compiler, ast, IntMul);
        case TokenSlashEqual:
            return compile_compound_assignment(compiler, ast, IntDiv);
        case TokenPercentEqual:
            return compile_compound_assignment(compiler, ast, IntRem);
        case TokenAndEqual:
            return compile_compound_assignment(compiler, ast, IntBand);
        case TokenOrEqual:
            return compile_compound_assignment(compiler, ast, IntBor);
        case TokenCircumflexEqual:
            return compile_compound_assignment(compiler, ast, IntBxor);
        case TokenLshiftEqual:
            return compile_compound_assignment(compiler, ast, IntLsl);
        case TokenRshiftEqual:
            return compile_compound_assignment(compiler, ast, IntLsr);
        case TokenPowerEqual:
            return compile_power_assignment(compiler, ast);
        case TokenEqual:
            return compile_simple_assignment(compiler, ast);
        default:
            return;
    }
}

void compile_assignment(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast->assignment.symbol);

    if (is_array_type(type.kind) && ast->assignment.operator.type != TokenEqual) {
        compile_array_compound_assignment(compiler, ast);

        return;
    }

    compile_assignment_operator(compiler, ast);
}

static void compile_power_assignment(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast->assignment.symbol);
    Operand left = load_variable_value(compiler, ast->assignment.symbol);
    left = materialize_operand(compiler, left);
    Operand right = compile_expression(compiler, ast->assignment.expr, false);
    Operand result = emit_binary_builtin(compiler, TokenPower, type, type, left, right);

    store_assignment_value(compiler, ast->assignment.symbol, result);
}

static Operand compile_zero_value(Compiler* compiler, size_t slots)
{
    if (slots == 2) {
        size_t position = make_i64_constant(compiler, 0);

        return make_wide_operand(emit_ldc_i64(compiler, position), true);
    }

    return make_operand(emit_ldi(compiler, 0), true);
}

static bool compile_unused_variable_definition(
    Compiler* compiler, ASTNode* ast, size_t slots, bool unused, bool preserve)
{
    if (!unused || preserve) {
        return false;
    }

    compile_expression(compiler, ast->variable_definition.expr, true);

    if (is_top_level_scope(ast->variable_definition.scope)) {
        return true;
    }

    allocate_registers(compiler, slots);

    return true;
}

static void initialize_global_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (!is_top_level_scope(ast->variable_definition.scope)) {
        return;
    }

    store_global_variable(compiler, ast, value);
    release_operand(compiler, value);
}

void compile_variable_definition(Compiler* compiler, ASTNode* ast)
{
    ast->variable_definition.register_position = compiler->register_count;
    Operand value;
    size_t slots = get_node_slot_count(ast);
    bool unused = ast->variable_definition.read_count == ast->variable_definition.discarded_read_count;
    bool constant_value = ast->variable_definition.constant && !ast->variable_definition.address_taken;
    bool preserve = compiler->repl_mode && is_compiling_top_level(compiler);

    if (compile_unused_variable_definition(compiler, ast, slots, unused || constant_value, preserve)) {
        return;
    }

    if (is_none(ast->variable_definition.expr)) {
        value = compile_zero_value(compiler, slots);
    } else {
        bool reference = get_reference_type(ast) != RefNone;
        value = compile_converted_expression(
            compiler, ast->variable_definition.expr, get_node_data_type(ast), reference);
    }

    if (!value.temporary) {
        int reg = allocate_registers(compiler, slots);

        emit_operand_mov(compiler, reg, value);

        value.reg = reg;
        value.temporary = true;
    }

    initialize_global_variable(compiler, ast, value);
}
