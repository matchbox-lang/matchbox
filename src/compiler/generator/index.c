#include "compiler/generator/index.h"
#include "builtin/builtin.h"
#include "common/array_type.h"
#include "common/tuple_type.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/memory.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/tuple.h"
#include "compiler/diagnostics.h"
#include "runtime/function_object.h"
#include "runtime/value.h"
#include <stddef.h>
#include <stdint.h>

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
    Type source = get_node_data_type(ast->index.array);

    if (ast->index.constant || is_slice_type(source.kind)) {
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

static void compile_index_bounds_check(Compiler* compiler, Operand index, Operand length)
{
    Type type = {.kind = TypeUsize};
    index.temporary = false;
    Operand condition = emit_numeric_comparison(compiler, TokenGreaterEqual, type, index, length);
    size_t valid_jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);
    compile_index_panic(compiler);
    patch_jump(compiler, valid_jump);
}

static Operand compile_runtime_index_offset(Compiler* compiler, ASTNode* ast, Operand buffer)
{
    Type type = get_node_data_type(ast->index.array);
    const ArrayType* info = type.array;
    size_t stride = get_type_storage_size(info->element_type);
    Type index_type = {.kind = TypeUsize};
    Operand index = compile_converted_expression(compiler, ast->index.index, index_type, false);

    index = materialize_operand(compiler, index);

    reserve_pointer_padding(compiler);

    Operand length;

    if (is_slice_type(type.kind)) {
        length = make_operand(buffer.reg + 1, false);
    } else {
        length = compile_usize_literal(compiler, info->count);
    }

    compile_index_bounds_check(compiler, index, length);
    scale_index(compiler, index, stride);
    release_pointer_padding(compiler);

    return index;
}

static void offset_buffer_by_runtime_index(Compiler* compiler, Operand buffer, ASTNode* ast)
{
    reserve_pointer_padding(compiler);

    Operand offset = compile_runtime_index_offset(compiler, ast, buffer);

    emit_buffer_offset(compiler, buffer, offset);
    release_pointer_padding(compiler);
}

static Operand compile_runtime_index_address(Compiler* compiler, ASTNode* ast)
{
    Operand buffer = compile_expression(compiler, ast->index.array, false);
    buffer = materialize_operand(compiler, buffer);

    offset_buffer_by_runtime_index(compiler, buffer, ast);

    if (buffer.pair) {
        release_register(compiler);

        buffer = make_operand(buffer.reg, true);
    }

    return buffer;
}

static size_t get_index_byte_offset(ASTNode* ast)
{
    Type type = get_node_data_type(ast->index.array);

    if (type.kind == TypeTuple) {
        return type.tuple->elements[ast->index.position].offset * sizeof(Value);
    }

    size_t stride = get_type_storage_size(type.array->element_type);

    return ast->index.position * stride;
}

static bool is_inline_index(ASTNode* ast)
{
    if (!is_constant_index(ast)) {
        return false;
    }

    Type source = get_node_data_type(ast->index.array);
    Type type = ast->index.type_id;
    bool pointer_element = is_array_type(type.kind) || get_reference_type(ast) != RefNone;

    return source.kind != TypeTuple || !pointer_element;
}

static ASTNode* get_index_base(ASTNode* ast, size_t* offset)
{
    ASTNode* base = ast;
    *offset = 0;

    while (is_inline_index(base)) {
        *offset += get_index_byte_offset(base);
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

    Type type = get_node_data_type(base);

    if (type.kind == TypeTuple) {
        return compile_tuple_address(compiler, base);
    }

    return compile_expression(compiler, base, false);
}

Operand compile_index_address(Compiler* compiler, ASTNode* ast)
{
    size_t offset;
    Operand buffer = compile_index_buffer(compiler, ast, &offset);

    return offset_buffer(compiler, buffer, offset);
}

static void compile_runtime_index_load(Compiler* compiler, ASTNode* ast, int destination)
{
    Operand buffer = compile_expression(compiler, ast->index.array, false);
    buffer = materialize_operand(compiler, buffer);

    Operand offset = compile_runtime_index_offset(compiler, ast, buffer);
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

static Operand compile_index_value(Compiler* compiler, ASTNode* ast, Operand buffer, size_t offset)
{
    Type type = ast->index.type_id;

    if (ast->index.operator.type == TokenEqual) {
        return compile_converted_expression(compiler, ast->index.value, type, false);
    }

    size_t slots = get_type_slot_count(type.kind);
    int reg = allocate_registers(compiler, slots);
    Opcode opcode = get_memory_load_opcode(type);
    Operand left = {reg, true, slots, false};

    emit_memory_access(compiler, opcode, reg, buffer.reg, offset);

    left = normalize_integer_operand(compiler, left, type);
    Operand right = compile_converted_expression(compiler, ast->index.value, type, false);
    TokenType operator = get_assignment_binary_token(ast->index.operator.type);

    if (operator == TokenPower) {
        return emit_binary_builtin(compiler, operator, type, type, left, right);
    }

    return emit_binary_operator(compiler, operator, type, left, right);
}

Operand compile_index(Compiler* compiler, ASTNode* ast)
{
    Type source = get_node_data_type(ast->index.array);

    if (source.kind == TypeTuple) {
        return compile_tuple_index(compiler, ast, false);
    }

    if (ast->index.type_id.kind == TypeTuple) {
        Type type = ast->index.type_id;
        size_t slots = get_value_slot_count(type);
        int reg = allocate_registers(compiler, slots);
        Operand buffer = compile_index_address(compiler, ast);
        Operand value = load_tuple_slots(compiler, buffer, 0, type);

        emit_operand_mov(compiler, reg, value);

        compiler->register_count = reg + slots;

        return make_typed_operand(reg, true, type);
    }

    if (is_array_type(ast->index.type_id.kind)) {
        return compile_index_address(compiler, ast);
    }

    size_t slots = get_type_slot_count(ast->index.type_id.kind);
    int destination = allocate_registers(compiler, slots);

    compile_index_load(compiler, ast, destination);

    Operand result = {destination, true, slots, false};

    return normalize_integer_operand(compiler, result, ast->index.type_id);
}

void compile_index_assignment(Compiler* compiler, ASTNode* ast)
{
    Type source = get_node_data_type(ast->index.array);

    if (source.kind == TypeTuple) {
        compile_tuple_index_assignment(compiler, ast);

        return;
    }

    size_t offset;
    Operand buffer = compile_index_buffer(compiler, ast, &offset);
    buffer = materialize_operand(compiler, buffer);
    Operand value = compile_index_value(compiler, ast, buffer, offset);

    store_array_value(compiler, buffer, value, offset, ast->index.type_id);
    release_operand(compiler, value);
    release_operand(compiler, buffer);
}

void compile_index_panic(Compiler* compiler)
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

void scale_index(Compiler* compiler, Operand index, size_t stride)
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
