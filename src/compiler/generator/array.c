#include "compiler/generator/array.h"
#include "builtin/memory.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/control_flow.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/variable.h"
#include "compiler/ast.h"
#include "compiler/diagnostics.h"
#include "runtime/function_object.h"
#include "runtime/value.h"
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

typedef struct ForOperands
{
    Operand source;
    Operand offset;
    Operand end;
    Operand index;
    size_t stride;
    size_t step;
} ForOperands;

static void compile_array_into(Compiler* compiler, ASTNode* ast, Operand array, size_t offset);

static Operand emit_array_copy(Compiler* compiler, const Builtin* definition, Type type, Operand source, bool discard)
{
    Type types[] = {type, {.kind = TypeUsize}};
    size_t size = get_array_storage_size(type);

    Builtin builtin = *definition;
    builtin.param_count = 2;
    builtin.params[1] = TypeUsize;

    Operand arguments[2];
    arguments[0] = source;
    arguments[1] = compile_usize_literal(compiler, size);

    return emit_builtin_operands(compiler, &builtin, types, arguments, 2, discard);
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

    size_t size = get_array_storage_size(type);
    size_t position = find_builtin_function_position(compiler, &builtin);

    if (position == SIZE_MAX) {
        position = add_builtin_function(compiler, &builtin);
    }

    FunctionObject* function = get_vector_at(&compiler->module->functions, position);
    CallArea call = open_call_area(compiler);

    compile_usize_literal(compiler, size);

    return close_call_area(compiler, function, position, call, false);
}

static void reserve_pointer_padding(Compiler* compiler)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        allocate_register(compiler);
    }
}

static void release_pointer_padding(Compiler* compiler)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        release_register(compiler);
    }
}

static Opcode get_pointer_add_opcode(void)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        return OpAddI64;
    }

    return OpAddI32;
}

static void emit_buffer_offset(Compiler* compiler, Operand buffer, Operand offset)
{
    Opcode opcode = get_pointer_add_opcode();

    reserve_pointer_padding(compiler);
    emit_instruction(compiler, opcode, buffer.reg, buffer.reg, offset.reg);
    release_pointer_padding(compiler);
    release_operand(compiler, offset);
}

static Operand offset_buffer(Compiler* compiler, Operand buffer, size_t offset)
{
    if (!offset) {
        return buffer;
    }

    buffer = materialize_operand(compiler, buffer);

    reserve_pointer_padding(compiler);

    Operand index = compile_usize_literal(compiler, offset);

    emit_buffer_offset(compiler, buffer, index);
    release_pointer_padding(compiler);

    return buffer;
}

static Opcode get_memory_load_opcode(Type type)
{
    size_t size = get_array_storage_size(type);

    switch (size) {
        case 1:
            return OpLdm8;
        case 2:
            return OpLdm16;
        case 4:
            return OpLdm32;
        default:
            return OpLdm64;
    }
}

static Opcode get_memory_store_opcode(Type type)
{
    size_t size = get_array_storage_size(type);

    switch (size) {
        case 1:
            return OpStm8;
        case 2:
            return OpStm16;
        case 4:
            return OpStm32;
        default:
            return OpStm64;
    }
}

static Opcode get_memory_immediate_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpLdm8:    return OpLdmi8;
        case OpLdm16:   return OpLdmi16;
        case OpLdm32:   return OpLdmi32;
        case OpLdm64:   return OpLdmi64;
        case OpStm8:    return OpStmi8;
        case OpStm16:   return OpStmi16;
        case OpStm32:   return OpStmi32;
        case OpStm64:   return OpStmi64;
        default:
            return opcode;
    }
}

static void emit_memory_access(Compiler* compiler, Opcode opcode, int value, int buffer, size_t offset)
{
    if (offset <= UINT8_MAX) {
        Opcode immediate = get_memory_immediate_opcode(opcode);

        emit_instruction(compiler, immediate, value, buffer, offset);

        return;
    }

    Operand index = compile_usize_literal(compiler, offset);

    emit_instruction(compiler, opcode, value, buffer, index.reg);
    release_operand(compiler, index);
}

static void copy_array_element(Compiler* compiler, Operand buffer, Operand source, size_t offset,
    size_t position, Type type)
{
    size_t slots = get_type_slot_count(type.kind);
    int destination = allocate_registers(compiler, slots);
    Opcode load = get_memory_load_opcode(type);
    Opcode store = get_memory_store_opcode(type);

    emit_memory_access(compiler, load, destination, source.reg, position);
    emit_memory_access(compiler, store, destination, buffer.reg, offset + position);

    Operand value = {destination, true, slots};

    release_operand(compiler, value);
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
    offsets.left_stride = get_array_storage_size(left_leaf);
    offsets.right_stride = get_array_storage_size(right_leaf);
    offsets.result_stride = get_array_storage_size(result_leaf);
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

static void advance_array_binary_offset_register(Compiler* compiler, Operand offset, size_t stride)
{
    Type type = {.kind = TypeUsize};
    Operand step = compile_usize_literal(compiler, stride);
    Opcode opcode = get_integer_opcode(type, IntAdd);

    reserve_pointer_padding(compiler);
    emit_instruction(compiler, opcode, offset.reg, offset.reg, step.reg);
    release_pointer_padding(compiler);
    release_operand(compiler, step);
}

static void advance_array_binary_offset(Compiler* compiler, Operand offset, size_t stride)
{
    if (stride > INT8_MAX) {
        advance_array_binary_offset_register(compiler, offset, stride);

        return;
    }

    Type type = {.kind = TypeUsize};
    Opcode opcode = get_width_opcode(type, OpAddiI64, OpAddiI32, OpAddiI16, OpAddiI8);

    emit_instruction(compiler, opcode, offset.reg, offset.reg, stride);
}

static void advance_array_binary_offsets(Compiler* compiler, const ArrayBinaryOffsets* offsets)
{
    advance_array_binary_offset(compiler, offsets->left, offsets->left_stride);

    if (offsets->right.reg != offsets->left.reg) {
        advance_array_binary_offset(compiler, offsets->right, offsets->right_stride);
    }

    if (offsets->result.reg != offsets->left.reg && offsets->result.reg != offsets->right.reg) {
        advance_array_binary_offset(compiler, offsets->result, offsets->result_stride);
    }
}

static Operand load_array_binary_element(Compiler* compiler, Operand buffer, Operand offset, Type type)
{
    size_t slots = get_type_slot_count(type.kind);
    int reg = allocate_registers(compiler, slots);
    Opcode opcode = get_memory_load_opcode(type);

    emit_instruction(compiler, opcode, reg, buffer.reg, offset.reg);

    Operand value = {reg, true, slots};
    value = normalize_integer_operand(compiler, value, type);
    value.temporary = false;

    return value;
}

static void compile_array_prefix_loop(Compiler* compiler, ASTNode* ast, Operand source, Operand result, Type type)
{
    Type leaf = get_array_leaf_type(type);
    size_t stride = get_array_storage_size(leaf);
    size_t size = get_array_storage_size(type);
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
    Operand element = load_array_binary_element(compiler, source, offset, leaf);
    Operand value = emit_prefix_operand(compiler, ast->prefix.operator.type, leaf, element);

    emit_instruction(compiler, store, value.reg, result.reg, offset.reg);
    advance_array_binary_offset(compiler, offset, stride);
    emit_loop_jump(compiler, start);
    patch_jump(compiler, exit_jump);
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
    Operand left_value = load_array_binary_element(compiler, left, offsets->left, left_leaf);
    Operand right_value = load_array_binary_element(compiler, right, offsets->right, right_leaf);
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
    size_t size = get_array_storage_size(left_type);
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

static void store_array_value(Compiler* compiler, Operand buffer, Operand value, size_t offset, Type type)
{
    if (!is_array_type(type.kind)) {
        Opcode opcode = get_memory_store_opcode(type);

        emit_memory_access(compiler, opcode, value.reg, buffer.reg, offset);

        return;
    }

    Type leaf = get_array_leaf_type(type);
    size_t size = get_array_storage_size(leaf);
    size_t total_size = get_array_storage_size(type);

    for (size_t position = 0; position < total_size; position += size) {
        copy_array_element(compiler, buffer, value, offset, position, leaf);
    }
}

static void compile_array_element(Compiler* compiler, ASTNode* ast, Operand array, size_t position, size_t offset)
{
    const ArrayType* info = ast->array.type_id.array;
    Type type = info->element_type;

    if (ast->array.flat) {
        type = get_array_leaf_type(ast->array.type_id);
    }

    size_t stride = get_array_storage_size(type);
    offset += position * stride;
    ASTNode* element = ast->array.elements.data[position];

    if (element->type == ASTArray) {
        compile_array_into(compiler, element, array, offset);

        return;
    }

    Operand value = compile_converted_expression(compiler, element, type, false);

    store_array_value(compiler, array, value, offset, type);
    release_operand(compiler, value);
}

static void compile_array_fill_loop(Compiler* compiler, Operand array, Operand value, Type type, size_t offset,
    size_t size)
{
    size_t stride = get_array_storage_size(type);
    Operand index = compile_usize_literal(compiler, offset);
    Operand end = compile_usize_literal(compiler, offset + size);
    Operand step = compile_usize_literal(compiler, stride);
    int condition = allocate_register(compiler);
    Opcode compare = OpCmpU32;
    Opcode add = get_pointer_add_opcode();
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
    size_t size = get_array_storage_size(type);

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

static void scale_array_index(Compiler* compiler, Operand index, size_t stride)
{
    if (stride == 1) {
        return;
    }

    Type type = {.kind = TypeUsize};

    if (stride <= INT8_MAX) {
        Opcode opcode = get_width_opcode(type, OpMuliI64, OpMuliI32, OpMuliI16, OpMuliI8);

        emit_instruction(compiler, opcode, index.reg, index.reg, stride);

        return;
    }

    Operand step = compile_usize_literal(compiler, stride);
    Opcode opcode = get_integer_opcode(type, IntMul);

    reserve_pointer_padding(compiler);
    emit_instruction(compiler, opcode, index.reg, index.reg, step.reg);
    release_pointer_padding(compiler);
    release_operand(compiler, step);
}

static uint64_t read_compiled_integer_constant(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast);
    size_t position = ast->prefix.constant_position;
    const Value* constants = compiler->module->constants.data;
    size_t size = get_integer_type_size(type.kind);

    if (size == sizeof(uint64_t)) {
        return read_u64(constants, position);
    }

    if (is_signed_integer_type(type.kind)) {
        return (uint64_t)read_signed_value(constants[position], type);
    }

    return read_unsigned_value(constants[position], type);
}

static void resolve_index_constant(Compiler* compiler, ASTNode* ast)
{
    if (ast->index.constant) {
        return;
    }

    ASTNode* expression = get_index_constant_expression(ast->index.index);

    if (!expression) {
        return;
    }

    if (!expression->prefix.evaluated) {
        evaluate_constant_expression(compiler, expression);
    }

    uint64_t value = read_compiled_integer_constant(compiler, expression);
    Type type = get_node_data_type(ast->index.array);

    if (value > SIZE_MAX || value >= type.array->count) {
        Token token = ast->index.token;

        fatal_source_error(token.line, token.column, "Array index is out of bounds");
    }

    ast->index.constant = true;
    ast->index.position = (size_t)value;
}

static void compile_index_panic(Compiler* compiler)
{
    static const char message[] = "Array index is out of bounds";
    size_t constant = find_string_constant(compiler->module, message, sizeof(message) - 1);

    if (constant == SIZE_MAX) {
        constant = add_string_constant(compiler->module, message, sizeof(message) - 1);
    }

    Type argument_types[] = {{.kind = TypeString}};
    const Builtin* builtin = resolve_builtin("panic", argument_types, 1);
    size_t position = find_builtin_function_position(compiler, builtin);

    if (position == SIZE_MAX) {
        position = add_builtin_function(compiler, builtin);
    }

    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    int reg = emit_ldc(compiler, constant);
    Operand argument = make_operand(reg, true);

    compile_call_with_argument(compiler, function, position, argument, true);
}

static void compile_index_bounds_check(Compiler* compiler, Operand index, size_t count)
{
    Type type = {.kind = TypeUsize};
    index.temporary = false;
    Operand length = compile_usize_literal(compiler, count);
    Operand condition = emit_numeric_comparison(compiler, TokenGreaterEqual, type, index, length);
    size_t valid_jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);
    compile_index_panic(compiler);
    patch_jump(compiler, valid_jump);
}

static Operand compile_runtime_index_offset(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast->index.array);
    const ArrayType* info = type.array;
    size_t stride = get_array_storage_size(info->element_type);
    Type index_type = {.kind = TypeUsize};
    Operand index = compile_converted_expression(compiler, ast->index.index, index_type, false);

    index = materialize_operand(compiler, index);

    reserve_pointer_padding(compiler);
    compile_index_bounds_check(compiler, index, info->count);
    scale_array_index(compiler, index, stride);
    release_pointer_padding(compiler);

    return index;
}

static void offset_buffer_by_runtime_index(Compiler* compiler, Operand buffer, ASTNode* ast)
{
    reserve_pointer_padding(compiler);

    Operand offset = compile_runtime_index_offset(compiler, ast);

    emit_buffer_offset(compiler, buffer, offset);
    release_pointer_padding(compiler);
}

static Operand compile_runtime_index_address(Compiler* compiler, ASTNode* ast)
{
    Operand buffer = compile_expression(compiler, ast->index.array, false);
    buffer = materialize_operand(compiler, buffer);
    offset_buffer_by_runtime_index(compiler, buffer, ast);

    return buffer;
}

static ASTNode* get_index_base(ASTNode* ast, size_t* offset)
{
    ASTNode* base = ast;
    *offset = 0;

    while (is_constant_index(base)) {
        Type type = get_node_data_type(base->index.array);
        const ArrayType* info = type.array;
        size_t stride = get_array_storage_size(info->element_type);
        *offset += base->index.position * stride;
        base = base->index.array;
    }

    return base;
}

static Operand compile_index_buffer(Compiler* compiler, ASTNode* ast, size_t* offset)
{
    resolve_index_constant(compiler, ast);

    ASTNode* base = get_index_base(ast, offset);
    if (base == ast) {
        return compile_runtime_index_address(compiler, ast);
    }

    return compile_expression(compiler, base, false);
}

static Operand compile_index_address(Compiler* compiler, ASTNode* ast)
{
    size_t offset;
    Operand buffer = compile_index_buffer(compiler, ast, &offset);

    return offset_buffer(compiler, buffer, offset);
}

static void compile_runtime_index_load(Compiler* compiler, ASTNode* ast, int destination)
{
    Operand buffer = compile_expression(compiler, ast->index.array, false);
    buffer = materialize_operand(compiler, buffer);

    Operand offset = compile_runtime_index_offset(compiler, ast);
    Opcode opcode = get_memory_load_opcode(ast->index.type_id);

    emit_instruction(compiler, opcode, destination, buffer.reg, offset.reg);
    release_operand(compiler, offset);
    release_operand(compiler, buffer);
}

static void compile_index_load(Compiler* compiler, ASTNode* ast, int destination)
{
    resolve_index_constant(compiler, ast);

    if (!ast->index.constant) {
        compile_runtime_index_load(compiler, ast, destination);

        return;
    }

    size_t offset;
    Operand buffer = compile_index_buffer(compiler, ast, &offset);
    Opcode opcode = get_memory_load_opcode(ast->index.type_id);

    emit_memory_access(compiler, opcode, destination, buffer.reg, offset);
    release_operand(compiler, buffer);
}

Operand compile_index(Compiler* compiler, ASTNode* ast)
{
    if (is_array_type(ast->index.type_id.kind)) {
        return compile_index_address(compiler, ast);
    }

    size_t slots = get_type_slot_count(ast->index.type_id.kind);
    int destination = allocate_registers(compiler, slots);

    compile_index_load(compiler, ast, destination);

    Operand result = {destination, true, slots};

    return normalize_integer_operand(compiler, result, ast->index.type_id);
}

void compile_index_assignment(Compiler* compiler, ASTNode* ast)
{
    size_t offset;
    Operand buffer = compile_index_buffer(compiler, ast, &offset);
    buffer = materialize_operand(compiler, buffer);
    Operand value = compile_converted_expression(compiler, ast->index.value, ast->index.type_id, false);

    store_array_value(compiler, buffer, value, offset, ast->index.type_id);
    release_operand(compiler, value);
    release_operand(compiler, buffer);
}

static size_t get_for_limit(ForOperands* loop, Type type)
{
    loop->stride = get_array_storage_size(type.array->element_type);
    loop->step = loop->stride;

    if (loop->stride) {
        return get_array_storage_size(type);
    }

    loop->step = 1;

    return type.array->count;
}

static void compile_for_index(Compiler* compiler, ASTNode* index, ForOperands* loop)
{
    loop->index = loop->offset;

    if (!index) {
        return;
    }

    if (loop->step != 1) {
        loop->index = compile_usize_literal(compiler, 0);
        loop->index.temporary = false;
    }

    int reg = loop->index.reg;

    if (index->variable_definition.address_taken) {
        size_t slots = get_node_slot_count(index);
        reg = allocate_registers(compiler, slots);
    }

    index->variable_definition.register_position = reg;
}

static ForOperands compile_for_operands(Compiler* compiler, ASTNode* ast, Type type)
{
    ForOperands loop;
    loop.source = compile_expression(compiler, ast->for_statement.array, false);
    loop.source = materialize_operand(compiler, loop.source);
    size_t limit = get_for_limit(&loop, type);
    loop.offset = compile_usize_literal(compiler, 0);
    loop.offset.temporary = false;
    loop.end = compile_usize_literal(compiler, limit);
    loop.end.temporary = false;

    compile_for_index(compiler, ast->for_statement.index, &loop);

    return loop;
}

static Operand compile_for_element_address(Compiler* compiler, const ForOperands* loop)
{
    Operand address = loop->source;
    address.temporary = false;
    address = materialize_operand(compiler, address);

    if (!loop->stride) {
        return address;
    }

    Opcode opcode = get_pointer_add_opcode();

    reserve_pointer_padding(compiler);
    emit_instruction(compiler, opcode, address.reg, address.reg, loop->offset.reg);
    release_pointer_padding(compiler);

    return address;
}

static Operand compile_for_array_element(Compiler* compiler, const ForOperands* loop, Type type)
{
    Operand address = compile_for_element_address(compiler, loop);
    const Builtin* builtin = resolve_builtin("copy", &type, 1);

    return emit_array_copy(compiler, builtin, type, address, false);
}

static Operand compile_for_element(Compiler* compiler, const ForOperands* loop, Type type)
{
    if (is_array_type(type.kind)) {
        return compile_for_array_element(compiler, loop, type);
    }

    Operand value = load_array_binary_element(compiler, loop->source, loop->offset, type);
    value.temporary = true;

    return value;
}

static void compile_for_value(Compiler* compiler, ASTNode* binding, const ForOperands* loop, Type type)
{
    if (!binding->variable_definition.read_count && !binding->variable_definition.address_taken) {
        return;
    }

    Type destination = binding->variable_definition.type_id;
    Operand value = compile_for_element(compiler, loop, type);
    value = convert_operand(compiler, value, type, destination);
    int reg = binding->variable_definition.register_position;

    emit_operand_to_register(compiler, reg, value);
    release_operand(compiler, value);
}

static void compile_for_index_value(Compiler* compiler, ASTNode* index, const ForOperands* loop)
{
    if (!index || !index->variable_definition.address_taken) {
        return;
    }

    int reg = index->variable_definition.register_position;

    emit_operand_to_register(compiler, reg, loop->index);
}

static void advance_for(Compiler* compiler, const ForOperands* loop)
{
    advance_array_binary_offset(compiler, loop->offset, loop->step);

    if (loop->index.reg != loop->offset.reg) {
        advance_array_binary_offset(compiler, loop->index, 1);
    }
}

static void compile_for_backedge(Compiler* compiler, const ForOperands* loop, size_t start)
{
    Operand condition;
    size_t jump;

    if (sizeof(void*) == sizeof(uint64_t)) {
        condition = emit_comparison(compiler, OpCmpU64, loop->offset, loop->end);
        jump = emit_jump(compiler, OpBnz, condition.reg);
    } else {
        condition = emit_numeric_comparison(
            compiler, TokenEqualEqual, (Type){.kind = TypeUsize}, loop->offset, loop->end);
        jump = emit_condition_jump(compiler, condition);
    }

    patch_jump_to(compiler, jump, start);
    release_operand(compiler, condition);
}

static void allocate_for_binding(Compiler* compiler, ASTNode* binding)
{
    size_t slots = get_node_slot_count(binding);
    binding->variable_definition.register_position = allocate_registers(compiler, slots);
}

static void compile_for_body(Compiler* compiler, ASTNode* ast, const ForOperands* operands, Type element_type)
{
    int body_register_count = compiler->register_count;
    LoopJumps loop;

    begin_loop(compiler, &loop);

    CodeObject* code = get_current_code_object(compiler);
    size_t start = count_code_object(code);
    compiler->peephole_barrier = start;

    compile_for_index_value(compiler, ast->for_statement.index, operands);
    compile_for_value(compiler, ast->for_statement.value, operands, element_type);
    compile_block_level_statements(compiler, &ast->for_statement.body->compound.statements);

    compiler->register_count = body_register_count;

    patch_loop_jumps(compiler, &loop.continues);
    advance_for(compiler, operands);
    compile_for_backedge(compiler, operands, start);
    end_loop(compiler, &loop);
}

void compile_for(Compiler* compiler, ASTNode* ast)
{
    int previous_register_count = compiler->register_count;
    Type type = get_node_data_type(ast->for_statement.array);

    if (!type.array->count) {
        compile_expression(compiler, ast->for_statement.array, true);

        return;
    }

    ForOperands operands = compile_for_operands(compiler, ast, type);

    allocate_for_binding(compiler, ast->for_statement.value);
    compile_for_body(compiler, ast, &operands, type.array->element_type);

    compiler->register_count = previous_register_count;
}
