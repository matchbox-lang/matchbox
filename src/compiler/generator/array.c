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
#include "compiler/generator/tuple.h"
#include "compiler/generator/variable.h"
#include "runtime/function_object.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ArrayBinaryOperation
{
    TokenType operator_type;
    Type left_type;
    Type right_type;
    Type operand_type;
    Type result_type;
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
    offsets.left_stride = 0;
    offsets.right_stride = 0;

    if (is_array_type(operation->left_type.kind)) {
        offsets.left_stride = get_type_storage_size(left_leaf);
    }

    if (is_array_type(operation->right_type.kind)) {
        offsets.right_stride = get_type_storage_size(right_leaf);
    }

    offsets.result_stride = get_type_storage_size(result_leaf);
    offsets.left = create_array_binary_offset(compiler, offsets.left_stride, SIZE_MAX, (Operand){0});
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

static void compile_array_prefix_loop(Compiler* compiler, TokenType operator_type, Operand source,
    Operand result, Type type)
{
    Type leaf = get_array_leaf_type(type);
    size_t stride = get_type_storage_size(leaf);
    size_t size = get_type_storage_size(type);
    Operand offset = compile_usize_literal(compiler, 0);
    offset.temporary = false;

    Operand end = compile_usize_literal(compiler, size);
    int condition = allocate_register(compiler);
    Opcode compare = get_three_way_opcode((Type){.kind = TypeUsize}, TokenEqualEqual);
    CodeObject* code = get_current_code_object(compiler);
    size_t start = count_code_object(code);

    compiler->peephole_barrier = start;
    emit_instruction(compiler, compare, condition, offset.reg, end.reg);

    size_t exit_jump = emit_jump(compiler, OpBz, condition);
    Operand element = load_memory_value(compiler, source, offset, leaf);
    Operand value = emit_prefix_operand(compiler, operator_type, leaf, element);

    store_memory_value_at_offset(compiler, result, value, offset, leaf);
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

    if (left_type.kind == TypeTuple || right_type.kind == TypeTuple) {
        return emit_tuple_binary_value(
            compiler, operator_type, left_type, right_type, result_type, operand_type, left, right);
    }

    if (operator_type == TokenPower || result_type.kind == TypeString) {
        return emit_binary_builtin(compiler, operator_type, left_type, right_type, left, right);
    }

    left = convert_operand(compiler, left, left_type, operand_type);
    right = convert_operand(compiler, right, right_type, operand_type);

    return emit_binary_operator(compiler, operator_type, operand_type, left, right);
}

static Operand load_array_binary_operand(Compiler* compiler, Operand source, Operand offset, Type type)
{
    if (!is_array_type(type.kind)) {
        source.temporary = false;

        return source;
    }

    Type leaf = get_array_leaf_type(type);

    return load_memory_value(compiler, source, offset, leaf);
}

static void compile_array_binary_body(Compiler* compiler, const ArrayBinaryOperation* operation,
    Operand left, Operand right, Operand result, const ArrayBinaryOffsets* offsets)
{
    Type left_type = operation->left_type;
    Type right_type = operation->right_type;
    Type left_leaf = get_array_leaf_type(left_type);
    Type right_leaf = get_array_leaf_type(right_type);
    Operand left_value = load_array_binary_operand(compiler, left, offsets->left, left_type);
    Operand right_value = load_array_binary_operand(compiler, right, offsets->right, right_type);
    Operand value = compile_array_leaf_binary(compiler, operation, left_value, right_value, left_leaf, right_leaf);
    bool equality = is_equality_token(operation->operator_type);

    if (equality) {
        emit_operand_mov(compiler, result.reg, value);

        return;
    }

    Type result_leaf = get_array_leaf_type(operation->result_type);

    store_memory_value_at_offset(compiler, result, value, offsets->result, result_leaf);
}

static void compile_array_binary_loop(Compiler* compiler, const ArrayBinaryOperation* operation,
    Operand left, Operand right, Operand result)
{
    bool left_array = is_array_type(operation->left_type.kind);
    Type shape = left_array ? operation->left_type : operation->right_type;
    size_t size = get_type_storage_size(shape);
    int previous_register_count = compiler->register_count;
    ArrayBinaryOffsets offsets = create_array_binary_offsets(compiler, operation);
    Operand position = left_array ? offsets.left : offsets.right;
    Operand end = compile_usize_literal(compiler, size);
    int condition = allocate_register(compiler);
    int body_register_count = compiler->register_count;
    Opcode compare = get_three_way_opcode((Type){.kind = TypeUsize}, TokenEqualEqual);
    CodeObject* code = get_current_code_object(compiler);
    size_t start = count_code_object(code);
    compiler->peephole_barrier = start;

    emit_instruction(compiler, compare, condition, position.reg, end.reg);
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

static Operand create_array_binary_result(Compiler* compiler, TokenType operator_type, Type type, bool same_shape)
{
    bool equality = is_equality_token(operator_type);

    if (equality) {
        int reg = emit_ldi(compiler, same_shape);

        return make_operand(reg, true);
    }

    return compile_array_new(compiler, type);
}

static void emit_array_binary_result(Compiler* compiler, const ArrayBinaryOperation* operation,
    Operand left, Operand right, Operand result, bool same_shape)
{
    if (same_shape) {
        compile_array_binary_loop(compiler, operation, left, right, result);
    }

    if (operation->operator_type == TokenNotEqual || operation->operator_type == TokenNotEqualEqual) {
        emit_instruction(compiler, OpNot, result.reg, result.reg, 0);
    }
}

Operand emit_array_binary(Compiler* compiler, TokenType operator_type, Type left_type, Type right_type,
    Type result_type, Type operand_type, Operand left, Operand right)
{
    bool broadcast = is_array_type(left_type.kind) != is_array_type(right_type.kind);
    bool same_shape = broadcast || array_shapes_equal(left_type, right_type);
    Operand result = create_array_binary_result(compiler, operator_type, result_type, same_shape);
    int previous_register_count = compiler->register_count;

    ArrayBinaryOperation operation = {
        .operator_type = operator_type,
        .left_type = left_type,
        .right_type = right_type,
        .operand_type = operand_type,
        .result_type = result_type
    };
    
    left.temporary = false;
    right.temporary = false;
    left = materialize_operand(compiler, left);
    right = materialize_operand(compiler, right);

    emit_array_binary_result(compiler, &operation, left, right, result, same_shape);

    compiler->register_count = previous_register_count;

    return result;
}

Operand compile_array_binary(Compiler* compiler, ASTNode* ast)
{
    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);
    TokenType operator_type = ast->binary.operator.type;
    bool broadcast = is_array_type(left_type.kind) != is_array_type(right_type.kind);
    bool same_shape = broadcast || array_shapes_equal(left_type, right_type);
    Operand result = create_array_binary_result(compiler, operator_type, ast->binary.type_id, same_shape);
    int previous_register_count = compiler->register_count;
    Operand left = compile_expression(compiler, ast->binary.left_expr, false);
    left = materialize_operand(compiler, left);
    Operand right = compile_expression(compiler, ast->binary.right_expr, false);
    right = materialize_operand(compiler, right);

    ArrayBinaryOperation operation = {
        .operator_type = operator_type,
        .left_type = left_type,
        .right_type = right_type,
        .operand_type = ast->binary.operand_type,
        .result_type = ast->binary.type_id
    };

    emit_array_binary_result(compiler, &operation, left, right, result, same_shape);

    compiler->register_count = previous_register_count;

    return result;
}

void compile_array_compound_assignment(Compiler* compiler, ASTNode* ast)
{
    Type left_type = get_node_data_type(ast->assignment.symbol);
    Type right_type = get_node_data_type(ast->assignment.expr);
    Type operand_type = get_array_leaf_type(left_type);
    TokenType operator_type = get_assignment_binary_token(ast->assignment.operator.type);
    ArrayBinaryOperation operation = {
        .operator_type = operator_type,
        .left_type = left_type,
        .right_type = right_type,
        .operand_type = operand_type,
        .result_type = left_type
    };
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

    compile_array_prefix_loop(compiler, ast->prefix.operator.type, source, result, type);
    compiler->register_count = previous_register_count;

    return result;
}

Operand emit_array_prefix(Compiler* compiler, TokenType operator_type, Type type, Operand source)
{
    Operand result = compile_array_new(compiler, type);
    int previous_register_count = compiler->register_count;
    source.temporary = false;
    source = materialize_operand(compiler, source);

    compile_array_prefix_loop(compiler, operator_type, source, result, type);

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
