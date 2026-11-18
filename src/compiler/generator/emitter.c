#include "compiler/generator/emitter.h"
#include "common/errors.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "runtime/code_object.h"
#include <stddef.h>
#include <stdint.h>

static _Noreturn void branch_offset_overflow_error(void)
{
    fatal_error("Branch offset exceeds instruction range");
}

CodeObject* current_code_object(Compiler* compiler)
{
    return &compiler->function->code;
}

static void emit_instruction_word(Compiler* compiler, Opcode opcode, int a, int b, int c)
{
    CodeObject* code = current_code_object(compiler);

    push_byte(code, opcode);
    push_byte(code, (uint8_t)a);
    push_byte(code, (uint8_t)b);
    push_byte(code, (uint8_t)c);
}

static int get_register_high_byte(Compiler* compiler, int operand, uint8_t flag, uint8_t field)
{
    if (!(flag & field)) {
        return 0;
    }

    check_register_count(compiler, operand + 1);

    return operand >> 8;
}

void emit_instruction(Compiler* compiler, Opcode opcode, int a, int b, int c)
{
    uint8_t flag = opcode_registers(opcode);

    int high_a = get_register_high_byte(compiler, a, flag, OpFlagRegA);
    int high_b = get_register_high_byte(compiler, b, flag, OpFlagRegB);
    int high_c = get_register_high_byte(compiler, c, flag, OpFlagRegC);

    if (high_a || high_b || high_c) {
        emit_instruction_word(compiler, OpWide, high_a, high_b, high_c);
    }

    emit_instruction_word(compiler, opcode, a, b, c);
}

void emit_hlt(Compiler* compiler)
{
    emit_instruction(compiler, OpHlt, 0, 0, 0);
}

static bool get_instruction_before(Compiler* compiler, size_t count, PreviousInstruction* instruction)
{
    CodeObject* code = current_code_object(compiler);

    if (count < INSTRUCTION_SIZE) {
        return false;
    }

    size_t start = count - INSTRUCTION_SIZE;
    
    if (start < compiler->peephole_barrier) {
        return false;
    }

    if (start >= INSTRUCTION_SIZE && code->data[start - INSTRUCTION_SIZE] == OpWide) {
        return false;
    }

    instruction->code = code;
    instruction->start = start;
    instruction->opcode = (Opcode)code->data[start];
    instruction->a = code->data[start + 1];
    instruction->b = code->data[start + 2];
    instruction->c = code->data[start + 3];

    return true;
}

bool get_previous_instruction(Compiler* compiler, PreviousInstruction* instruction)
{
    CodeObject* code = current_code_object(compiler);
    size_t count = count_code_object(code);

    return get_instruction_before(compiler, count, instruction);
}

void emit_mov(Compiler* compiler, int destination, int source)
{
    if (destination == source) {
        return;
    }

    emit_instruction(compiler, OpMov, destination, source, 0);
}

void emit_operand_mov(Compiler* compiler, int destination, Operand operand)
{
    if (operand.slots != 2) {
        emit_mov(compiler, destination, operand.reg);
        return;
    }

    if (destination == operand.reg) {
        return;
    }

    emit_instruction(compiler, OpMov64, destination, operand.reg, 0);
}

int emit_ldc(Compiler* compiler, uint16_t imm)
{
    int reg = allocate_register(compiler);

    emit_instruction(compiler, OpLdc, reg, imm >> 8, imm);

    return reg;
}

int emit_ldc_i64(Compiler* compiler, uint16_t position)
{
    int reg = allocate_registers(compiler, 2);

    emit_instruction(compiler, OpLdc64, reg, position >> 8, position);

    return reg;
}

int emit_ldi(Compiler* compiler, int16_t imm)
{
    int reg = allocate_register(compiler);

    emit_instruction(compiler, OpLdi, reg, imm >> 8, imm);

    return reg;
}

void emit_global_instruction(Compiler* compiler, Opcode opcode, int reg, size_t position)
{
    if (position > UINT16_MAX) {
        global_index_overflow_error();
    }

    emit_instruction(compiler, opcode, reg, position >> 8, position);
}

int emit_ldg(Compiler* compiler, size_t position)
{
    int reg = allocate_register(compiler);

    emit_global_instruction(compiler, OpLdg, reg, position);

    return reg;
}

void emit_call_instruction(Compiler* compiler, int frame_register, uint16_t function_position)
{
    check_register_count(compiler, frame_register + 1);

    emit_instruction(compiler, OpCall, frame_register, function_position >> 8, function_position);
}

void emit_ret(Compiler* compiler)
{
    emit_instruction(compiler, OpRet, 0, 0, 0);
}

void emit_retv(Compiler* compiler, Operand operand)
{
    Opcode opcode = operand.slots == 2 ? OpRet64 : OpRetv;

    emit_instruction(compiler, opcode, operand.reg, 0, 0);
}

bool retarget_temporary(Compiler* compiler, Operand value, int destination)
{
    PreviousInstruction previous;

    if (!value.temporary || destination > (int)UINT8_MAX) {
        return false;
    }

    if (!get_previous_instruction(compiler, &previous) || previous.a != value.reg) {
        return false;
    }

    if (!opcode_writes_a(previous.opcode)) {
        return false;
    }

    set_byte_at(previous.code, previous.start + 1, destination);

    return true;
}

size_t emit_jump(Compiler* compiler, Opcode opcode, int condition_register)
{
    size_t position = count_code_object(current_code_object(compiler));
    emit_instruction(compiler, opcode, condition_register, 0, 0);

    return position;
}

static Opcode get_jump_comparison_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpBeq:     return OpNe;
        case OpBeqi:    return OpNei;
        case OpBne:     return OpEq;
        case OpBnei:    return OpEqi;
        case OpBltI32:  return OpLeI32;
        case OpBltU32:  return OpLeU32;
        case OpBleI32:  return OpLtI32;
        case OpBleU32:  return OpLtU32;
        default:
            return OpHlt;
    }
}

static Opcode expand_condition_jump(CodeObject* code, size_t position, Opcode opcode, size_t offset)
{
    if (opcode < OpBeq || opcode > OpBleU32 || offset <= INT8_MAX) {
        return opcode;
    }

    size_t comparison = position - INSTRUCTION_SIZE;
    Opcode comparison_opcode = get_jump_comparison_opcode(opcode);

    set_byte_at(code, comparison, comparison_opcode);
    set_byte_at(code, position, OpBz);
    set_byte_at(code, position + 1, code->data[comparison + 1]);

    return OpBz;
}

static void check_jump_offset(Opcode opcode, size_t offset)
{
    size_t maximum = INT32_MAX >> 8;

    if (opcode == OpBz || opcode == OpBnz) {
        maximum = INT16_MAX;
    }

    if (opcode >= OpBeq && opcode <= OpBleU32) {
        maximum = INT8_MAX;
    }

    if (offset > maximum) {
        branch_offset_overflow_error();
    }
}

void patch_jump(Compiler* compiler, size_t position)
{
    CodeObject* code = current_code_object(compiler);

    if (code->data[position] == OpWide) {
        position += INSTRUCTION_SIZE;
    }

    size_t count = count_code_object(code);
    size_t word_offset = (count - position - INSTRUCTION_SIZE) / INSTRUCTION_SIZE;
    Opcode opcode = (Opcode)code->data[position];
    compiler->peephole_barrier = count;
    opcode = expand_condition_jump(code, position, opcode, word_offset);

    check_jump_offset(opcode, word_offset);

    int offset = (int)word_offset;

    if (opcode == OpBz || opcode == OpBnz) {
        set_byte_at(code, position + 2, offset >> 8);
        set_byte_at(code, position + 3, offset);

        return;
    }

    if (opcode >= OpBeq && opcode <= OpBleU32) {
        set_byte_at(code, position + 3, offset);

        return;
    }

    set_byte_at(code, position + 1, offset >> 16);
    set_byte_at(code, position + 2, offset >> 8);
    set_byte_at(code, position + 3, offset);
}

static Opcode get_condition_jump_opcode(Opcode comparison)
{
    switch (comparison) {
        case OpEq:      return OpBne;
        case OpEqi:     return OpBnei;
        case OpNe:      return OpBeq;
        case OpNei:     return OpBeqi;
        case OpLtI32:   return OpBleI32;
        case OpLtU32:   return OpBleU32;
        case OpLeI32:   return OpBltI32;
        case OpLeU32:   return OpBltU32;
        default:
            return OpHlt;
    }
}

size_t emit_condition_jump(Compiler* compiler, Operand condition)
{
    PreviousInstruction comparison;

    if (!condition.temporary || !get_previous_instruction(compiler, &comparison) || comparison.a != condition.reg) {
        return emit_jump(compiler, OpBz, condition.reg);
    }

    Opcode opcode = get_condition_jump_opcode(comparison.opcode);

    if (opcode == OpHlt) {
        return emit_jump(compiler, OpBz, condition.reg);
    }

    size_t position = comparison.start + INSTRUCTION_SIZE;
    int left = comparison.b;
    int right = comparison.c;

    if (opcode >= OpBltI32 && opcode <= OpBleU32) {
        left = comparison.c;
        right = comparison.b;
    }

    set_byte_at(comparison.code, comparison.start, OpNop);
    emit_instruction(compiler, opcode, left, right, 0);

    return position;
}

void emit_loop_jump(Compiler* compiler, size_t target)
{
    CodeObject* code = current_code_object(compiler);
    size_t position = count_code_object(code);
    size_t distance = (position + INSTRUCTION_SIZE - target) / INSTRUCTION_SIZE;

    check_jump_offset(OpJmp, distance);

    int offset = -(int)distance;

    emit_instruction(compiler, OpJmp, (offset >> 16) & 255, (offset >> 8) & 255, offset & 255);
}
