#ifndef GENERATOR_EMITTER_H
#define GENERATOR_EMITTER_H

#include "compiler/generator/operand.h"
#include "runtime/code_object.h"
#include "vm/opcode.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct PreviousInstruction
{
    CodeObject* code;
    size_t start;
    Opcode opcode;
    uint8_t a;
    uint8_t b;
    uint8_t c;
} PreviousInstruction;

CodeObject* current_code_object(Compiler* compiler);
void emit_instruction(Compiler* compiler, Opcode opcode, int a, int b, int c);
void emit_hlt(Compiler* compiler);
bool get_previous_instruction(Compiler* compiler, PreviousInstruction* instruction);
void emit_mov(Compiler* compiler, int destination, int source);
void emit_operand_mov(Compiler* compiler, int destination, Operand operand);
int emit_ldc(Compiler* compiler, uint16_t imm);
int emit_ldc_i64(Compiler* compiler, uint16_t position);
int emit_ldi(Compiler* compiler, int16_t imm);
void emit_global_instruction(Compiler* compiler, Opcode opcode, int reg, size_t position);
int emit_ldg(Compiler* compiler, size_t position);
void emit_call_instruction(Compiler* compiler, int frame_register, uint16_t function_position);
void emit_ret(Compiler* compiler);
void emit_retv(Compiler* compiler, Operand operand);
bool retarget_temporary(Compiler* compiler, Operand value, int destination);
void emit_operand_to_register(Compiler* compiler, int destination, Operand value);
size_t emit_jump(Compiler* compiler, Opcode opcode, int condition_register);
void patch_jump(Compiler* compiler, size_t position);
size_t emit_condition_jump(Compiler* compiler, Operand condition);
void emit_loop_jump(Compiler* compiler, size_t target);

#endif
