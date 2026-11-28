#include "compiler/generator/array.h"
#include "builtin/memory.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/query.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/memory.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/variable.h"
#include "runtime/function_object.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ArrayBinaryOperation
{
    TokenType operator_type;
    Type left_type;
    Type right_type;
    Type result_type;
    Type operand_type;
} ArrayBinaryOperation;

typedef struct ArrayBinaryOffsets
{
    Operand left;
    Operand right;
    Operand result;
    size_t left_stride;
    size_t right_stride;
    size_t result_stride;
} ArrayBinaryOffsets;

static void compile_array_into(Compiler* compiler, ASTNode* ast, Operand array, size_t offset);

static Operand compile_array_new(Compiler* compiler, Type type)
{
    static const Builtin builtin = {
        .name = "create_array",
        .entry = builtin_create_array,
        .param_count = 1,
        .params = {TypeUsize},
        .return_type_id = TypeArray,
        .constant_evaluable = true
    };

    size_t size = get_type_storage_size(type);
    size_t position = find_builtin_function_position(compiler, &builtin);

    if (position == SIZE_MAX) {
        position = add_builtin_function(compiler, &builtin);
    }

    FunctionObject* function = get_vector_at(&compiler->module->functions, position);
    CallArea call = open_call_area(compiler);

    compile_usize_literal(compiler, size);

    return close_call_area(compiler, function, position, call, false);
}

static Operand create_array_binary_offset(Compiler* compiler, size_t stride,
    size_t shared_stride, Operand shared_offset)
{
    if (stride == shared_stride) {
        return shared_offset;
    }

    Operand offset = compile_usize_literal(compiler, 0);
    offset.temporary = false;

    return offset;
}

static ArrayBinaryOffsets create_array_binary_offsets(Compiler* compiler, const ArrayBinaryOperation* operation)
{
    Type left_leaf = get_array_leaf_type(operation->left_type);
    Type right_leaf = get_array_leaf_type(operation->right_type);
    Type result_leaf = get_array_leaf_type(operation->result_type);

    ArrayBinaryOffsets offsets;
    offsets.left_stride = get_type_storage_size(left_leaf);
    offsets.right_stride = get_type_storage_size(right_leaf);
    offsets.result_stride = get_type_storage_size(result_leaf);
    offsets.left = create_array_binary_offset(compiler, offsets.left_stride, 0, (Operand){0});
    offsets.right = create_array_binary_offset(compiler, offsets.right_stride, offsets.left_stride, offsets.left);
    offsets.result = offsets.left;

    if (is_equality_token(operation->operator_type)) {
        offsets.result_stride = offsets.left_stride;

        return offsets;
    }

    if (offsets.result_stride == offsets.right_stride) {
        offsets.result = offsets.right;

        return offsets;
    }

    offsets.result = create_array_binary_offset(compiler, offsets.result_stride, offsets.left_stride, offsets.left);

    return offsets;
}

static void advance_array_binary_offsets(Compiler* compiler, const ArrayBinaryOffsets* offsets)
{
    advance_offset(compiler, offsets->left, offsets->left_stride);

    if (offsets->right.reg != offsets->left.reg) {
        advance_offset(compiler, offsets->right, offsets->right_stride);
    }

    if (offsets->result.reg != offsets->left.reg && offsets->result.reg != offsets->right.reg) {
        advance_offset(compiler, offsets->result, offsets->result_stride);
    }
}

static void compile_array_prefix_loop(Compiler* compiler, ASTNode* ast, Operand source, Operand result, Type type)
{
    Type leaf = get_array_leaf_type(type);
    size_t stride = get_type_storage_size(leaf);
    size_t size = get_type_storage_size(type);
    Operand offset = compile_usize_literal(compiler, 0);
    offset.temporary = false;

    Operand end = compile_usize_literal(compiler, size);
    int condition = allocate_register(compiler);
    Opcode compare = get_three_way_opcode((Type){.kind = TypeUsize}, TokenEqualEqual);
    Opcode store = get_memory_store_opcode(leaf);
    CodeObject* code = get_current_code_object(compiler);
    size_t start = count_code_object(code);

    compiler->peephole_barrier = start;
    emit_instruction(compiler, compare, condition, offset.reg, end.reg);

    size_t exit_jump = emit_jump(compiler, OpBz, condition);
    Operand element = load_memory_value(compiler, source, offset, leaf);
    Operand value = emit_prefix_operand(compiler, ast->prefix.operator.type, leaf, element);

    emit_instruction(compiler, store, value.reg, result.reg, offset.reg);
    advance_offset(compiler, offset, stride);
    emit_loop_jump(compiler, start);
    patch_jump(compiler, exit_jump);
}

static Operand compile_array_leaf_binary(Compiler* compiler, const ArrayBinaryOperation* operation,
    Operand left, Operand right, Type left_type, Type right_type)
{
    TokenType operator_type = operation->operator_type;
    Type result_type = get_array_leaf_type(operation->result_type);
    Type operand_type = operation->operand_type;

    if (is_equality_token(operator_type)) {
        operator_type = TokenEqualEqual;
    }

    if (operator_type == TokenPower || result_type.kind == TypeString) {
        return emit_binary_builtin(compiler, operator_type, left_type, right_type, left, right);
    }

    left = convert_operand(compiler, left, left_type, operand_type);
    right = convert_operand(compiler, right, right_type, operand_type);

    return emit_binary_operator(compiler, operator_type, operand_type, left, right);
}

static void compile_array_binary_body(Compiler* compiler, const ArrayBinaryOperation* operation,
    Operand left, Operand right, Operand result, const ArrayBinaryOffsets* offsets)
{
    Type left_type = operation->left_type;
    Type right_type = operation->right_type;
    Type left_leaf = get_array_leaf_type(left_type);
    Type right_leaf = get_array_leaf_type(right_type);
    Operand left_value = load_memory_value(compiler, left, offsets->left, left_leaf);
    Operand right_value = load_memory_value(compiler, right, offsets->right, right_leaf);
    Operand value = compile_array_leaf_binary(compiler, operation, left_value, right_value, left_leaf, right_leaf);
    bool equality = is_equality_token(operation->operator_type);

    if (equality) {
        emit_operand_mov(compiler, result.reg, value);

        return;
    }

    Type result_leaf = get_array_leaf_type(operation->result_type);
    Opcode store = get_memory_store_opcode(result_leaf);

    emit_instruction(compiler, store, value.reg, result.reg, offsets->result.reg);
}

static void compile_array_binary_loop(Compiler* compiler, const ArrayBinaryOperation* operation,
    Operand left, Operand right, Operand result)
{
    Type left_type = operation->left_type;
    size_t size = get_type_storage_size(left_type);
    int previous_register_count = compiler->register_count;
    ArrayBinaryOffsets offsets = create_array_binary_offsets(compiler, operation);
    Operand end = compile_usize_literal(compiler, size);
    int condition = allocate_register(compiler);
    int body_register_count = compiler->register_count;
    Opcode compare = get_three_way_opcode((Type){.kind = TypeUsize}, TokenEqualEqual);
    CodeObject* code = get_current_code_object(compiler);
    size_t start = count_code_object(code);
    compiler->peephole_barrier = start;

    emit_instruction(compiler, compare, condition, offsets.left.reg, end.reg);
    size_t exit_jump = emit_jump(compiler, OpBz, condition);

    compile_array_binary_body(compiler, operation, left, right, result, &offsets);

    compiler->register_count = body_register_count;
    bool equality = is_equality_token(operation->operator_type);
    size_t unequal_jump = 0;

    if (equality) {
        unequal_jump = emit_jump(compiler, OpBz, result.reg);
    }

    advance_array_binary_offsets(compiler, &offsets);
    emit_loop_jump(compiler, start);
    patch_jump(compiler, exit_jump);

    if (equality) {
        patch_jump(compiler, unequal_jump);
    }

    compiler->register_count = previous_register_count;
}

static void compile_array_element(Compiler* compiler, ASTNode* ast, Operand array, size_t position, size_t offset)
{
    const ArrayType* info = ast->array.type_id.array;
    Type type = info->element_type;

    if (ast->array.flat) {
        type = get_array_leaf_type(ast->array.type_id);
    }

    size_t stride = get_type_storage_size(type);
    offset += position * stride;
    ASTNode* element = ast->array.elements.data[position];

    if (element->type == ASTArray) {
        compile_array_into(compiler, element, array, offset);

        return;
    }

    Operand value = compile_converted_expression(compiler, element, type, false);

    store_memory_value(compiler, array, value, offset, type);
    release_operand(compiler, value);
}

static void compile_array_fill_loop(Compiler* compiler, Operand array, Operand value, Type type, size_t offset,
    size_t size)
{
    size_t stride = get_type_storage_size(type);
    Operand index = compile_usize_literal(compiler, offset);
    Operand end = compile_usize_literal(compiler, offset + size);
    Operand step = compile_usize_literal(compiler, stride);
    int condition = allocate_register(compiler);
    Opcode compare = OpCmpU32;
    Type index_type = {.kind = TypeUsize};
    Opcode add = get_integer_opcode(index_type, IntAdd);
    Opcode store = get_memory_store_opcode(type);

    if (sizeof(void*) == sizeof(uint64_t)) {
        compare = OpCmpU64;
    }

    CodeObject* code = get_current_code_object(compiler);
    size_t start = count_code_object(code);
    compiler->peephole_barrier = start;

    emit_instruction(compiler, compare, condition, index.reg, end.reg);

    size_t exit_jump = emit_jump(compiler, OpBz, condition);

    emit_instruction(compiler, store, value.reg, array.reg, index.reg);
    emit_instruction(compiler, add, index.reg, index.reg, step.reg);
    emit_loop_jump(compiler, start);
    patch_jump(compiler, exit_jump);
    release_register(compiler);
    release_operand(compiler, step);
    release_operand(compiler, end);
    release_operand(compiler, index);
}

static void compile_array_fill_into(Compiler* compiler, ASTNode* ast, Operand array, size_t offset)
{
    Type type = ast->array.type_id;
    Type leaf = get_array_leaf_type(type);
    ASTNode* expression = ast->array.elements.data[0];
    Operand value = compile_converted_expression(compiler, expression, leaf, false);
    size_t size = get_type_storage_size(type);

    compile_array_fill_loop(compiler, array, value, leaf, offset, size);
    release_operand(compiler, value);
}

static void compile_array_into(Compiler* compiler, ASTNode* ast, Operand array, size_t offset)
{
    if (ast->array.fill) {
        compile_array_fill_into(compiler, ast, array, offset);

        return;
    }

    for (size_t i = 0; i < ast->array.elements.count; i++) {
        compile_array_element(compiler, ast, array, i, offset);
    }
}

Operand compile_array(Compiler* compiler, ASTNode* ast)
{
    Operand array = compile_array_new(compiler, ast->array.type_id);

    compile_array_into(compiler, ast, array, 0);

    return array;
}

Operand compile_array_binary(Compiler* compiler, ASTNode* ast)
{
    bool equality = is_equality_token(ast->binary.operator.type);
    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);
    bool same_shape = array_shapes_equal(left_type, right_type);
    Operand result;

    if (equality) {
        int reg = emit_ldi(compiler, same_shape);
        result = make_operand(reg, true);
    } else {
        result = compile_array_new(compiler, ast->binary.type_id);
    }

    int previous_register_count = compiler->register_count;
    Operand left = compile_expression(compiler, ast->binary.left_expr, false);
    left = materialize_operand(compiler, left);
    Operand right = compile_expression(compiler, ast->binary.right_expr, false);
    right = materialize_operand(compiler, right);

    ArrayBinaryOperation operation = {
        ast->binary.operator.type, left_type, right_type, ast->binary.type_id, ast->binary.operand_type
    };

    if (same_shape) {
        compile_array_binary_loop(compiler, &operation, left, right, result);
    }

    compiler->register_count = previous_register_count;
    TokenType operator_type = ast->binary.operator.type;

    if (operator_type == TokenNotEqual || operator_type == TokenNotEqualEqual) {
        emit_instruction(compiler, OpNot, result.reg, result.reg, 0);
    }

    return result;
}

void compile_array_compound_assignment(Compiler* compiler, ASTNode* ast)
{
    Type left_type = get_node_data_type(ast->assignment.symbol);
    Type right_type = get_node_data_type(ast->assignment.expr);
    Type operand_type = get_array_leaf_type(left_type);
    TokenType operator_type = get_assignment_binary_token(ast->assignment.operator.type);
    ArrayBinaryOperation operation = {operator_type, left_type, right_type, left_type, operand_type};
    int previous_register_count = compiler->register_count;
    Operand left = load_variable_value(compiler, ast->assignment.symbol);
    left = materialize_operand(compiler, left);
    Operand right = compile_expression(compiler, ast->assignment.expr, false);
    right = materialize_operand(compiler, right);

    compile_array_binary_loop(compiler, &operation, left, right, left);

    compiler->register_count = previous_register_count;
}

Operand compile_array_copy(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Type type = get_node_data_type(argument);
    Operand source = compile_expression(compiler, argument, false);

    return emit_array_copy(compiler, ast->builtin_call.builtin, type, source, discard);
}

Operand compile_array_length(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Type type = get_node_data_type(argument);

    compile_expression(compiler, argument, true);

    if (discard) {
        return no_operand();
    }

    return compile_usize_literal(compiler, type.array->count);
}

Operand compile_array_prefix(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast);
    Operand result = compile_array_new(compiler, type);
    int previous_register_count = compiler->register_count;
    Operand source = compile_expression(compiler, ast->prefix.expr, false);
    source = materialize_operand(compiler, source);

    compile_array_prefix_loop(compiler, ast, source, result, type);
    compiler->register_count = previous_register_count;

    return result;
}

Operand emit_array_copy(Compiler* compiler, const Builtin* definition, Type type, Operand source, bool discard)
{
    Type types[] = {type, {.kind = TypeUsize}};
    size_t size = get_type_storage_size(type);

    Builtin builtin = *definition;
    builtin.param_count = 2;
    builtin.params[1] = TypeUsize;

    Operand arguments[2];
    arguments[0] = source;
    arguments[1] = compile_usize_literal(compiler, size);

    return emit_builtin_operands(compiler, &builtin, types, arguments, 2, discard);
}
