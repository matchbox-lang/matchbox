#include "compiler/generator/memory.h"
#include "common/array_type.h"
#include "common/type.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/tuple.h"
#include "vm/opcode.h"
#include <stdint.h>

static void advance_offset_register(Compiler* compiler, Operand offset, size_t stride)
{
    Type type = {.kind = TypeUsize};
    Operand step = compile_usize_literal(compiler, stride);
    Opcode opcode = get_integer_opcode(type, IntAdd);

    reserve_pointer_padding(compiler);
    emit_instruction(compiler, opcode, offset.reg, offset.reg, step.reg);
    release_pointer_padding(compiler);
    release_operand(compiler, step);
}

void advance_offset(Compiler* compiler, Operand offset, size_t stride)
{
    if (stride > INT8_MAX) {
        advance_offset_register(compiler, offset, stride);

        return;
    }

    Type type = {.kind = TypeUsize};
    Opcode opcode = get_width_opcode(type, OpAddiI64, OpAddiI32, OpAddiI16, OpAddiI8);

    emit_instruction(compiler, opcode, offset.reg, offset.reg, stride);
}

void emit_memory_access(Compiler* compiler, Opcode opcode, int value, int buffer, size_t offset)
{
    if (offset <= UINT8_MAX) {
        Opcode immediate = get_immediate_opcode(opcode);

        emit_instruction(compiler, immediate, value, buffer, offset);

        return;
    }

    Operand index = compile_usize_literal(compiler, offset);

    emit_instruction(compiler, opcode, value, buffer, index.reg);
    release_operand(compiler, index);
}

static Operand offset_memory_buffer(Compiler* compiler, Operand buffer, Operand offset)
{
    buffer.temporary = false;
    offset.temporary = false;
    buffer = materialize_operand(compiler, buffer);

    emit_buffer_offset(compiler, buffer, offset);

    return buffer;
}

static Operand load_tuple_memory_value(Compiler* compiler, Operand buffer, Operand offset, Type type)
{
    Operand address = offset_memory_buffer(compiler, buffer, offset);
    Operand value = load_tuple_slots(compiler, address, 0, type);

    emit_operand_mov(compiler, address.reg, value);

    value.reg = address.reg;
    value.temporary = false;
    compiler->register_count = value.reg + value.slots;

    return value;
}

void store_memory_value_at_offset(Compiler* compiler, Operand buffer, Operand value, Operand offset, Type type)
{
    if (type.kind == TypeTuple) {
        Operand address = offset_memory_buffer(compiler, buffer, offset);

        store_tuple_slots(compiler, address, value, 0);
        release_operand(compiler, address);

        return;
    }

    Opcode store = get_memory_store_opcode(type);

    emit_instruction(compiler, store, value.reg, buffer.reg, offset.reg);
}

Operand load_memory_value(Compiler* compiler, Operand buffer, Operand offset, Type type)
{
    if (type.kind == TypeTuple) {
        return load_tuple_memory_value(compiler, buffer, offset, type);
    }

    size_t slots = get_type_slot_count(type.kind);
    int reg = allocate_registers(compiler, slots);
    Opcode opcode = get_memory_load_opcode(type);

    emit_instruction(compiler, opcode, reg, buffer.reg, offset.reg);

    Operand value = {reg, true, slots, false};
    value = normalize_integer_operand(compiler, value, type);
    value.temporary = false;

    return value;
}

Opcode get_memory_load_opcode(Type type)
{
    size_t size = get_type_storage_size(type);

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

Opcode get_memory_store_opcode(Type type)
{
    size_t size = get_type_storage_size(type);

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

static void copy_memory_element(Compiler* compiler, Operand buffer, Operand source, size_t offset,
    size_t position, Type type)
{
    if (type.kind == TypeTuple) {
        Operand value = load_tuple_slots(compiler, source, position, type);

        store_tuple_slots(compiler, buffer, value, offset + position);
        release_operand(compiler, value);

        return;
    }

    size_t slots = get_type_slot_count(type.kind);
    int destination = allocate_registers(compiler, slots);
    Opcode load = get_memory_load_opcode(type);
    Opcode store = get_memory_store_opcode(type);

    emit_memory_access(compiler, load, destination, source.reg, position);
    emit_memory_access(compiler, store, destination, buffer.reg, offset + position);

    Operand value = {destination, true, slots, false};

    release_operand(compiler, value);
}

void store_memory_value(Compiler* compiler, Operand buffer, Operand value, size_t offset, Type type)
{
    if (type.kind == TypeTuple) {
        store_tuple_slots(compiler, buffer, value, offset);

        return;
    }

    if (!is_array_type(type.kind)) {
        Opcode opcode = get_memory_store_opcode(type);

        emit_memory_access(compiler, opcode, value.reg, buffer.reg, offset);

        return;
    }

    Type leaf = get_array_leaf_type(type);
    size_t size = get_type_storage_size(leaf);
    size_t total_size = get_type_storage_size(type);

    for (size_t position = 0; position < total_size; position += size) {
        copy_memory_element(compiler, buffer, value, offset, position, leaf);
    }
}
