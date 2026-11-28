#include "compiler/generator/tuple.h"
#include "common/tuple_type.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/index.h"
#include "compiler/generator/memory.h"
#include "compiler/generator/variable.h"
#include "runtime/value.h"

static void emit_tuple_binary(Compiler* compiler, const BinaryOperation* operation,
    Operand result, Operand left, Operand right);

static void compile_tuple_element(Compiler* compiler, ASTNode* ast, Operand result, size_t position)
{
    TupleElement element = ast->tuple.type_id.tuple->elements[position];
    ASTNode* expression = ast->tuple.elements.data[position];
    bool reference = element.reference != RefNone;
    Operand value = compile_converted_expression(compiler, expression, element.type, reference);

    emit_operand_mov(compiler, result.reg + element.offset, value);
    release_operand(compiler, value);
}

static Operand emit_tuple_binary_leaf(Compiler* compiler, const BinaryOperation* operation,
    Operand left, Operand right)
{
    TokenType operator_type = operation->operator_type;

    if (is_equality_token(operator_type)) {
        operator_type = TokenEqualEqual;
    }

    if (is_array_type(operation->left_type.kind) || is_array_type(operation->right_type.kind)) {
        return emit_array_binary(compiler, operator_type, operation->left_type, operation->right_type,
            operation->result_type, operation->operand_type, left, right);
    }

    if (operator_type == TokenPower || operation->result_type.kind == TypeString) {
        left = materialize_operand(compiler, left);
        right = materialize_operand(compiler, right);

        return emit_binary_builtin(
            compiler, operator_type, operation->left_type, operation->right_type, left, right);
    }

    left = convert_operand(compiler, left, operation->left_type, operation->operand_type);
    right = convert_operand(compiler, right, operation->right_type, operation->operand_type);

    return emit_binary_operator(compiler, operator_type, operation->operand_type, left, right);
}

static void emit_tuple_binary_leaf_result(Compiler* compiler, const BinaryOperation* operation,
    Operand result, Operand left, Operand right)
{
    Operand value = emit_tuple_binary_leaf(compiler, operation, left, right);
    bool equality = is_equality_token(operation->operator_type);

    if (equality) {
        emit_instruction(compiler, OpBandI32, result.reg, result.reg, value.reg);
    } else {
        emit_operand_mov(compiler, result.reg, value);
    }

    release_operand(compiler, value);
}

static void emit_tuple_binary_element(Compiler* compiler, const BinaryOperation* operation,
    Operand result, Operand left, Operand right, size_t index)
{
    TupleElement left_element = get_tuple_operand_element(operation->left_type, index);
    TupleElement right_element = get_tuple_operand_element(operation->right_type, index);
    BinaryOperation element = *operation;
    element.operand_type = operation->operand_type.tuple->elements[index].type;
    element.left_type = left_element.type;
    element.right_type = right_element.type;
    Operand destination = result;
    Operand left_value = make_typed_operand(left.reg + left_element.offset, false, left_element.type);
    Operand right_value = make_typed_operand(right.reg + right_element.offset, false, right_element.type);
    bool equality = is_equality_token(operation->operator_type);

    if (!equality) {
        TupleElement result_element = operation->result_type.tuple->elements[index];
        element.result_type = result_element.type;
        destination = make_typed_operand(result.reg + result_element.offset, false, result_element.type);
    }

    emit_tuple_binary(compiler, &element, destination, left_value, right_value);
}

static void emit_tuple_binary(Compiler* compiler, const BinaryOperation* operation,
    Operand result, Operand left, Operand right)
{
    if (is_array_type(operation->left_type.kind) || is_array_type(operation->right_type.kind)) {
        emit_tuple_binary_leaf_result(compiler, operation, result, left, right);

        return;
    }

    if (operation->operand_type.kind != TypeTuple) {
        emit_tuple_binary_leaf_result(compiler, operation, result, left, right);

        return;
    }

    for (size_t i = 0; i < operation->operand_type.tuple->count; i++) {
        emit_tuple_binary_element(compiler, operation, result, left, right, i);
    }
}

Operand emit_tuple_binary_value(Compiler* compiler, TokenType operator_type, Type left_type, Type right_type,
    Type result_type, Type operand_type, Operand left, Operand right)
{
    size_t slots = get_value_slot_count(result_type);
    int reg = allocate_registers(compiler, slots);
    Operand result = make_typed_operand(reg, true, result_type);
    int previous_register_count = compiler->register_count;
    bool equality = is_equality_token(operator_type);
    bool broadcast = left_type.kind != TypeTuple || right_type.kind != TypeTuple;
    bool same_shape = broadcast || tuple_shapes_equal(left_type, right_type);

    BinaryOperation operation = {
        .operator_type = operator_type,
        .left_type = left_type,
        .right_type = right_type,
        .operand_type = operand_type,
        .result_type = result_type
    };

    if (equality) {
        emit_instruction(compiler, OpLdi, result.reg, 0, same_shape);
    }

    if (same_shape) {
        emit_tuple_binary(compiler, &operation, result, left, right);
    }

    if (operator_type == TokenNotEqual || operator_type == TokenNotEqualEqual) {
        emit_instruction(compiler, OpNot, result.reg, result.reg, 0);
    }

    compiler->register_count = previous_register_count;

    return result;
}

Operand compile_tuple_binary(Compiler* compiler, ASTNode* ast)
{
    Type type = ast->binary.type_id;
    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);
    int start = compiler->register_count;
    Operand left = compile_expression(compiler, ast->binary.left_expr, false);
    left = materialize_operand(compiler, left);
    Operand right = compile_expression(compiler, ast->binary.right_expr, false);
    Operand result = emit_tuple_binary_value(
        compiler, ast->binary.operator.type, left_type, right_type, type, ast->binary.operand_type, left, right);

    emit_operand_mov(compiler, start, result);

    result.reg = start;
    compiler->register_count = start + result.slots;

    return result;
}

static void emit_tuple_prefix_element(Compiler* compiler, TokenType operator_type, TupleElement element,
    Operand result, Operand source)
{
    Operand value = make_typed_operand(source.reg + element.offset, false, element.type);
    value = emit_prefix_operand(compiler, operator_type, element.type, value);

    emit_operand_mov(compiler, result.reg + element.offset, value);
    release_operand(compiler, value);
}

Operand emit_tuple_prefix(Compiler* compiler, TokenType operator_type, Type type, Operand source)
{
    size_t slots = get_value_slot_count(type);
    int reg = allocate_registers(compiler, slots);
    Operand result = make_typed_operand(reg, true, type);

    for (size_t i = 0; i < type.tuple->count; i++) {
        emit_tuple_prefix_element(compiler, operator_type, type.tuple->elements[i], result, source);
    }

    return result;
}

Operand compile_tuple(Compiler* compiler, ASTNode* ast)
{
    Type type = ast->tuple.type_id;
    size_t slots = get_value_slot_count(type);
    int reg = allocate_registers(compiler, slots);
    Operand result = make_typed_operand(reg, true, type);

    for (size_t i = 0; i < ast->tuple.elements.count; i++) {
        compile_tuple_element(compiler, ast, result, i);
    }

    return result;
}

void store_tuple_slots(Compiler* compiler, Operand buffer, Operand value, size_t offset)
{
    Opcode store = sizeof(Value) == 8 ? OpStm64 : OpStm32;

    for (size_t i = 0; i < value.slots; i++) {
        emit_memory_access(compiler, store, value.reg + i, buffer.reg, offset + i * sizeof(Value));
    }
}

Operand load_tuple_slots(Compiler* compiler, Operand buffer, size_t offset, Type type)
{
    size_t slots = get_value_slot_count(type);
    int reg = allocate_registers(compiler, slots);
    Opcode load = sizeof(Value) == 8 ? OpLdm64 : OpLdm32;

    for (size_t i = 0; i < slots; i++) {
        emit_memory_access(compiler, load, reg + i, buffer.reg, offset + i * sizeof(Value));
    }

    return make_typed_operand(reg, true, type);
}

Operand compile_tuple_index(Compiler* compiler, ASTNode* ast, bool reference)
{
    Type source_type = get_node_data_type(ast->index.array);
    TupleElement element = source_type.tuple->elements[ast->index.position];
    Type type = element.type;
    size_t slots = get_tuple_element_slot_count(element);

    int reg = allocate_registers(compiler, slots);
    Operand tuple = compile_expression(compiler, ast->index.array, false);
    Operand value = make_typed_operand(tuple.reg + element.offset, false, type);
    value.slots = slots;

    emit_operand_mov(compiler, reg, value);
    release_operand(compiler, tuple);

    value.reg = reg;
    value.temporary = true;

    if (!reference && element.reference != RefNone && !is_slice_type(type.kind)) {
        size_t count = get_value_slot_count(type);

        return dereference_operand(compiler, value, count);
    }

    return value;
}

Operand compile_tuple_address(Compiler* compiler, ASTNode* ast)
{
    if (get_reference_type(ast) != RefNone) {
        return compile_reference_expression(compiler, ast);
    }

    if (ast->type == ASTVariable) {
        return reference_variable(compiler, ast->variable.symbol);
    }

    return compile_index_address(compiler, ast);
}

static Operand compile_tuple_assignment_value(Compiler* compiler, ASTNode* ast)
{
    Type type = ast->index.type_id;

    if (ast->index.operator.type == TokenEqual) {
        bool reference = get_reference_type(ast) != RefNone;

        return compile_converted_expression(compiler, ast->index.value, type, reference);
    }

    Operand left = compile_tuple_index(compiler, ast, false);
    Operand right = compile_converted_expression(compiler, ast->index.value, type, false);
    TokenType operator = get_assignment_binary_token(ast->index.operator.type);

    return emit_binary_operator(compiler, operator, type, left, right);
}

void compile_tuple_index_assignment(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast->index.array);
    size_t offset = type.tuple->elements[ast->index.position].offset * sizeof(Value);
    Operand buffer;
    bool mutation = get_reference_type(ast) == RefExclusive && ast->index.operator.type != TokenEqual;

    if (mutation) {
        buffer = compile_tuple_index(compiler, ast, true);
        offset = 0;
    } else {
        buffer = compile_tuple_address(compiler, ast->index.array);
    }
    Operand value = compile_tuple_assignment_value(compiler, ast);

    if (mutation && ast->index.type_id.kind != TypeTuple) {
        Opcode store = value.slots == 2 ? OpStr64 : OpStr;

        emit_instruction(compiler, store, value.reg, buffer.reg, 0);
    } else {
        store_tuple_slots(compiler, buffer, value, offset);
    }

    release_operand(compiler, value);
    release_operand(compiler, buffer);
}

static Operand copy_tuple_element(Compiler* compiler, const Builtin* builtin, TupleElement element, Operand source)
{
    if (element.reference != RefNone) {
        source.slots = get_tuple_element_slot_count(element);

        return source;
    }

    if (element.type.kind == TypeTuple) {
        return emit_tuple_copy(compiler, builtin, element.type, source);
    }

    if (is_array_type(element.type.kind)) {
        return emit_array_copy(compiler, builtin, element.type, source, false);
    }

    return source;
}

Operand emit_tuple_copy(Compiler* compiler, const Builtin* builtin, Type type, Operand source)
{
    size_t slots = get_value_slot_count(type);
    int reg = allocate_registers(compiler, slots);
    Operand result = make_typed_operand(reg, true, type);

    for (size_t i = 0; i < type.tuple->count; i++) {
        TupleElement element = type.tuple->elements[i];
        Operand value = make_typed_operand(source.reg + element.offset, false, element.type);
        value = copy_tuple_element(compiler, builtin, element, value);

        emit_operand_mov(compiler, reg + element.offset, value);
        release_operand(compiler, value);
    }

    return result;
}

Operand compile_tuple_copy(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Type type = get_node_data_type(argument);
    int start = compiler->register_count;
    Operand source = compile_expression(compiler, argument, false);
    Operand result = emit_tuple_copy(compiler, ast->builtin_call.builtin, type, source);

    emit_operand_mov(compiler, start, result);

    result.reg = start;
    compiler->register_count = start + result.slots;

    if (discard) {
        release_operand(compiler, result);

        return no_operand();
    }

    return result;
}

Operand compile_tuple_length(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Type type = get_node_data_type(argument);

    compile_expression(compiler, argument, true);

    if (discard) {
        return no_operand();
    }

    return compile_usize_literal(compiler, type.tuple->count);
}
