#include "compiler/generator/tuple.h"
#include "common/tuple_type.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/generator/array.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/index.h"
#include "compiler/generator/memory.h"
#include "compiler/generator/variable.h"
#include "runtime/value.h"

static void compile_tuple_element(Compiler* compiler, ASTNode* ast, Operand result, size_t position)
{
    TupleElement element = ast->tuple.type_id.tuple->elements[position];
    ASTNode* expression = ast->tuple.elements.data[position];
    bool reference = element.reference != RefNone;
    Operand value = compile_converted_expression(compiler, expression, element.type, reference);

    emit_operand_mov(compiler, result.reg + element.offset, value);
    release_operand(compiler, value);
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
