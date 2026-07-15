#ifndef OPCODE_H
#define OPCODE_H

#include <stdint.h>

typedef uint32_t Instruction;

#define OPCODE(inst) ((uint8_t)(inst))
#define OPERAND_A(inst) ((uint8_t)((inst) >> 8))
#define OPERAND_B(inst) ((uint8_t)((inst) >> 16))
#define OPERAND_C(inst) ((uint8_t)((inst) >> 24))
#define OPERAND_BC(inst) ((uint16_t)((OPERAND_B(inst) << 8) | OPERAND_C(inst)))
#define OPERAND_ABC(inst) ((uint32_t)((OPERAND_A(inst) << 16) | (OPERAND_B(inst) << 8) | OPERAND_C(inst)))
#define SIGNED_OPERAND_ABC(inst) ((int32_t)(OPERAND_ABC(inst) ^ 0x800000u) - 0x800000)

#define LOAD_CALL_FUNCTION_BITS 6
#define LOAD_CALL_FUNCTION_MASK 0x3fu
#define LOAD_CALL_OPERAND_MASK 0x3ffu
#define LOAD_CALL_SIGNED_OPERAND_MIN -512
#define LOAD_CALL_SIGNED_OPERAND_MAX 511

#define LOAD_CALL_FUNCTION(operands) ((uint8_t)((operands) & LOAD_CALL_FUNCTION_MASK))
#define LOAD_CALL_OPERAND(operands) ((uint16_t)((operands) >> LOAD_CALL_FUNCTION_BITS))
#define LOAD_CALL_SIGNED_OPERAND(operands) \
    ((int16_t)((int32_t)((LOAD_CALL_OPERAND(operands) ^ 0x200u)) - 0x200))

#define INSTRUCTION_SIZE 4
#define OPERAND_A_OFFSET 1

typedef enum OpcodeFlag
{
    OP_FLAG_NONE = 0,
    OP_FLAG_WRITES_A = 1 << 0
} OpcodeFlag;

typedef enum Opcode
{
    OP_HLT,     // HLT
    OP_NOP,     // NOP
    OP_MOV,     // MOV A, B
    OP_LDC,     // LDC A, imm16
    OP_LDG,     // LDG A, imm16
    OP_LDI,     // LDI A, imm16
    OP_STG,     // STG A, imm16
    OP_ADD,     // ADD A, B, C
    OP_SUB,     // SUB A, B, C
    OP_MUL,     // MUL A, B, C
    OP_DIV,     // DIV A, B, C
    OP_REM,     // REM A, B, C
    OP_POW,     // POW A, B, C
    OP_BAND,    // BAND A, B, C
    OP_BOR,     // BOR A, B, C
    OP_BXOR,    // BXOR A, B, C
    OP_BNOT,    // BNOT A, B
    OP_LSL,     // LSL A, B, C
    OP_LSR,     // LSR A, B, C
    OP_ASR,     // ASR A, B, C
    OP_NOT,     // NOT A, B
    OP_NEG,     // NEG A, B
    OP_BEQ,     // BEQ A, B, imm8
    OP_BLT,     // BLT A, B, imm8
    OP_BLE,     // BLE A, B, imm8
    OP_JMP,     // JMP imm24
    OP_CALL,    // CALL A, imm16
    OP_LDC_CALL, // LDC_CALL A, imm10, imm6
    OP_LDI_CALL, // LDI_CALL A, imm10, imm6
    OP_LDG_CALL, // LDG_CALL A, imm10, imm6
    OP_RET,     // RET
    OP_RETV     // RETV A
} Opcode;

OpcodeFlag getOpcodeFlags(Opcode opcode);

#endif
