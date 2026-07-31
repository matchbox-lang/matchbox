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
    OP_REFL,
    OP_REFG,
    OP_LDR,
    OP_STR,

    // 64-bit data movement
    OP_MOV_64,
    OP_LDC_64,
    OP_LDG_64,
    OP_STG_64,
    OP_LDR_64,
    OP_STR_64,

    // Addition
    OP_ADD_I64,
    OP_ADDI_I64,
    OP_ADD_I32,
    OP_ADDI_I32,
    OP_ADD_I16,
    OP_ADDI_I16,
    OP_ADD_I8,
    OP_ADDI_I8,

    // Subtraction
    OP_SUB_I64,
    OP_SUBI_I64,
    OP_SUB_I32,
    OP_SUBI_I32,
    OP_SUB_I16,
    OP_SUBI_I16,
    OP_SUB_I8,
    OP_SUBI_I8,

    // Multiplication
    OP_MUL_I64,
    OP_MULI_I64,
    OP_MUL_I32,
    OP_MULI_I32,
    OP_MUL_I16,
    OP_MULI_I16,
    OP_MUL_I8,
    OP_MULI_I8,

    // Integer division
    OP_IDIV_I64,
    OP_IDIV_U64,
    OP_IDIV_I32,
    OP_IDIV_U32,
    OP_IDIV_I16,
    OP_IDIV_U16,
    OP_IDIV_I8,
    OP_IDIV_U8,

    // Remainder
    OP_REM_I64,
    OP_REM_U64,
    OP_REM_I32,
    OP_REM_U32,
    OP_REM_I16,
    OP_REM_U16,
    OP_REM_I8,
    OP_REM_U8,

    // Bitwise AND
    OP_BAND_I64,
    OP_BANDI_I64,
    OP_BAND_I32,
    OP_BANDI_I32,
    OP_BAND_I16,
    OP_BANDI_I16,
    OP_BAND_I8,
    OP_BANDI_I8,

    // Bitwise OR
    OP_BOR_I64,
    OP_BORI_I64,
    OP_BOR_I32,
    OP_BORI_I32,
    OP_BOR_I16,
    OP_BORI_I16,
    OP_BOR_I8,
    OP_BORI_I8,

    // Bitwise XOR
    OP_BXOR_I64,
    OP_BXORI_I64,
    OP_BXOR_I32,
    OP_BXORI_I32,
    OP_BXOR_I16,
    OP_BXORI_I16,
    OP_BXOR_I8,
    OP_BXORI_I8,

    // Bitwise NOT
    OP_BNOT_I64,
    OP_BNOT_I32,
    OP_BNOT_I16,
    OP_BNOT_I8,

    // Logical shift left
    OP_LSL_I64,
    OP_LSLI_I64,
    OP_LSL_I32,
    OP_LSLI_I32,
    OP_LSL_I16,
    OP_LSLI_I16,
    OP_LSL_I8,
    OP_LSLI_I8,

    // Logical shift right
    OP_LSR_I64,
    OP_LSRI_I64,
    OP_LSR_I32,
    OP_LSRI_I32,
    OP_LSR_I16,
    OP_LSRI_I16,
    OP_LSR_I8,
    OP_LSRI_I8,

    // Arithmetic shift right
    OP_ASR_I64,
    OP_ASRI_I64,
    OP_ASR_I32,
    OP_ASRI_I32,
    OP_ASR_I16,
    OP_ASRI_I16,
    OP_ASR_I8,
    OP_ASRI_I8,

    // Logical
    OP_AND,
    OP_OR,
    OP_NOT,

    // Negation
    OP_NEG,
    OP_NEG_I64,

    // Comparisons
    OP_EQ,
    OP_NE,
    OP_LT_INT,
    OP_LT_UINT,
    OP_LE_INT,
    OP_LE_UINT,

    // Branching
    OP_BZ,
    OP_BNZ,
    OP_BEQ,
    OP_BNE,
    OP_BLT_INT,
    OP_BLT_UINT,
    OP_BLE_INT,
    OP_BLE_UINT,
    OP_JMP,

    // Functions
    OP_CALL,
    OP_RET,
    OP_RET_64,
    OP_RETV,

    // Repeated instructions
    OP_CALL2,
    OP_LDI2,
    OP_LDG2,
    OP_MOV2,

    // Fused calls
    OP_LDI_CALL,
    OP_LDC_CALL,
    OP_LDG_CALL,
    OP_LDR_CALL,

    // Fused data movement
    OP_LDI_STG,
    OP_LDI_LDG,
    OP_LDG_LDI,

    OP_COUNT
} Opcode;

OpcodeFlag getOpcodeFlags(Opcode opcode);

#endif
