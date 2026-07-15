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
    // Control
    OP_HLT,
    OP_NOP,

    // Data movement
    OP_MOV,
    OP_LDI,
    OP_LDC,
    OP_LDG,
    OP_STG,

    // Arithmetic
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_REM,
    OP_POW,

    // Bitwise
    OP_BAND,
    OP_BOR,
    OP_BXOR,
    OP_BNOT,
    OP_LSL,
    OP_LSR,
    OP_ASR,

    // Unary
    OP_NOT,
    OP_NEG,

    // Branching
    OP_BEQ,
    OP_BLT,
    OP_BLE,
    OP_JMP,

    // Functions
    OP_CALL,
    OP_RET,
    OP_RETV,

    // Superinstructions
    OP_LDC_CALL,
    OP_LDG_CALL,
    OP_LDI_CALL,
    OP_CALL_CALL,
    OP_LDI2,
    OP_LDI_STG,

    OP_COUNT
} Opcode;

OpcodeFlag getOpcodeFlags(Opcode opcode);

#endif
