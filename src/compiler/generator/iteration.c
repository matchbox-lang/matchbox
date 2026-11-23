#include "compiler/generator/iteration.h"
#include "builtin/builtin.h"
#include "common/array_type.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/query.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/control_flow.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/index.h"
#include "compiler/generator/memory.h"
#include "compiler/generator/operand.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ForOperands
{
    Operand source;
    Operand offset;
    Operand end;
    Operand index;
    size_t stride;
    size_t step;
    bool range;
} ForOperands;

static size_t get_for_limit(ForOperands* loop, Type type)
{
    loop->stride = get_type_storage_size(type.array->element_type);
    loop->step = loop->stride;

    if (loop->stride) {
        return get_type_storage_size(type);
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

    if (loop->step != 1 || loop->range) {
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
    ForOperands loop = {0};
    loop.source = compile_expression(compiler, ast->for_statement.array, false);
    loop.source = materialize_operand(compiler, loop.source);
    size_t limit = get_for_limit(&loop, type);
    loop.offset = compile_usize_literal(compiler, 0);
    loop.offset.temporary = false;

    if (is_slice_type(type.kind)) {
        Operand length = make_operand(loop.source.reg + 1, false);
        loop.end = materialize_operand(compiler, length);

        scale_index(compiler, loop.end, loop.step);
    } else {
        loop.end = compile_usize_literal(compiler, limit);
    }

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

    Type type = {.kind = TypeUsize};
    Opcode opcode = get_integer_opcode(type, IntAdd);

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
    if (loop->range) {
        return loop->offset;
    }

    if (is_array_type(type.kind)) {
        return compile_for_array_element(compiler, loop, type);
    }

    Operand value = load_memory_value(compiler, loop->source, loop->offset, type);
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
    advance_offset(compiler, loop->offset, loop->step);

    if (loop->index.reg != loop->offset.reg) {
        advance_offset(compiler, loop->index, 1);
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

static ForOperands compile_range_operands(Compiler* compiler, ASTNode* ast)
{
    Type type = {.kind = TypeUsize};
    ForOperands loop = {.step = 1, .range = true};

    loop.offset = compile_converted_expression(compiler, ast->for_statement.array, type, false);
    loop.offset = materialize_operand(compiler, loop.offset);
    loop.offset.temporary = false;
    loop.end = compile_converted_expression(compiler, ast->for_statement.end, type, false);
    loop.end = materialize_operand(compiler, loop.end);
    loop.end.temporary = false;

    compile_for_index(compiler, ast->for_statement.index, &loop);

    return loop;
}

static ForOperands compile_for_header(Compiler* compiler, ASTNode* ast, Type type, Type* element_type)
{
    if (ast->for_statement.end) {
        *element_type = (Type){.kind = TypeUsize};

        return compile_range_operands(compiler, ast);
    }

    *element_type = type.array->element_type;

    return compile_for_operands(compiler, ast, type);
}

static size_t compile_for_entry(Compiler* compiler, const ForOperands* operands)
{
    Type type = {.kind = TypeUsize};
    Operand condition = emit_numeric_comparison(compiler, TokenLess, type, operands->offset, operands->end);
    size_t skip = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);

    return skip;
}

void compile_for(Compiler* compiler, ASTNode* ast)
{
    int previous_register_count = compiler->register_count;
    Type type = get_node_data_type(ast->for_statement.array);

    if (!ast->for_statement.end && type.kind == TypeArray && !type.array->count) {
        compile_expression(compiler, ast->for_statement.array, true);

        return;
    }

    Type element_type;
    ForOperands operands = compile_for_header(compiler, ast, type, &element_type);
    size_t skip = compile_for_entry(compiler, &operands);

    allocate_for_binding(compiler, ast->for_statement.value);
    compile_for_body(compiler, ast, &operands, element_type);
    patch_jump(compiler, skip);

    compiler->register_count = previous_register_count;
}
