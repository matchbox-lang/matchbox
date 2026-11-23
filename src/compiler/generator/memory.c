#include "compiler/generator/memory.h"
#include "common/array_type.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
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

Operand load_memory_value(Compiler* compiler, Operand buffer, Operand offset, Type type)
{
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

Opcode get_memory_store_opcode(Type type)
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
