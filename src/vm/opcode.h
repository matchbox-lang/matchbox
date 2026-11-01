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
    OpFlagNone = 0,
    OpFlagWritesA = 1 << 0
} OpcodeFlag;

typedef enum OpcodeRegisterFlag
{
    OpRegisterNone = 0,
    OpRegisterA = 1 << 0,
    OpRegisterB = 1 << 1,
    OpRegisterC = 1 << 2
} OpcodeRegisterFlag;

typedef enum Opcode
{
    // Control
    OpHlt,
    OpNop,

    // Data movement
    OpMov,
    OpLdi,
    OpLdc,
    OpLdg,
    OpStg,
    OpStgi,
    OpRefl,
    OpRefg,
    OpLdr,
    OpStr,

    // 64-bit data movement
    OpMov64,
    OpLdc64,
    OpLdg64,
    OpStg64,
    OpLdr64,
    OpStr64,

    // Integer extension
    OpSextI32,
    OpZextI32,

    // Floating-point conversion
    OpF32ToF64,
    OpF64ToF32,

    // Floating-point to integer conversion
    OpF32ToI32,
    OpF32ToU32,
    OpF32ToI64,
    OpF32ToU64,
    OpF64ToI32,
    OpF64ToU32,
    OpF64ToI64,
    OpF64ToU64,

    // Integer to floating-point conversion
    OpI64ToF32,
    OpI64ToF64,
    OpU64ToF32,
    OpU64ToF64,
    OpI32ToF32,
    OpI32ToF64,
    OpU32ToF32,
    OpU32ToF64,

    // Floating-point arithmetic
    OpAddF32,
    OpAddF64,
    OpSubF32,
    OpSubF64,
    OpMulF32,
    OpMulF64,
    OpDivF32,
    OpDivF64,
    OpRemF32,
    OpRemF64,
    OpNegF32,
    OpNegF64,

    // Integer addition
    OpAddI64,
    OpAddiI64,
    OpAddI32,
    OpAddiI32,
    OpAddI16,
    OpAddiI16,
    OpAddI8,
    OpAddiI8,

    // Integer subtraction
    OpSubI64,
    OpSubiI64,
    OpSubI32,
    OpSubiI32,
    OpSubI16,
    OpSubiI16,
    OpSubI8,
    OpSubiI8,

    // Integer multiplication
    OpMulI64,
    OpMuliI64,
    OpMulI32,
    OpMuliI32,
    OpMulI16,
    OpMuliI16,
    OpMulI8,
    OpMuliI8,

    // Integer division
    OpIdivI64,
    OpIdivU64,
    OpIdivI32,
    OpIdivU32,
    OpIdivI16,
    OpIdivU16,
    OpIdivI8,
    OpIdivU8,

    // Integer remainder
    OpRemI64,
    OpRemU64,
    OpRemI32,
    OpRemU32,
    OpRemI16,
    OpRemU16,
    OpRemI8,
    OpRemU8,

    // Bitwise AND
    OpBandI64,
    OpBandiI64,
    OpBandI32,
    OpBandiI32,
    OpBandI16,
    OpBandiI16,
    OpBandI8,
    OpBandiI8,

    // Bitwise OR
    OpBorI64,
    OpBoriI64,
    OpBorI32,
    OpBoriI32,
    OpBorI16,
    OpBoriI16,
    OpBorI8,
    OpBoriI8,

    // Bitwise XOR
    OpBxorI64,
    OpBxoriI64,
    OpBxorI32,
    OpBxoriI32,
    OpBxorI16,
    OpBxoriI16,
    OpBxorI8,
    OpBxoriI8,

    // Bitwise NOT
    OpBnotI64,
    OpBnotI32,
    OpBnotI16,
    OpBnotI8,

    // Logical shift left
    OpLslI64,
    OpLsliI64,
    OpLslI32,
    OpLsliI32,
    OpLslI16,
    OpLsliI16,
    OpLslI8,
    OpLsliI8,

    // Logical shift right
    OpLsrI64,
    OpLsriI64,
    OpLsrI32,
    OpLsriI32,
    OpLsrI16,
    OpLsriI16,
    OpLsrI8,
    OpLsriI8,

    // Arithmetic shift right
    OpAsrI64,
    OpAsriI64,
    OpAsrI32,
    OpAsriI32,
    OpAsrI16,
    OpAsriI16,
    OpAsrI8,
    OpAsriI8,

    // Logical
    OpAnd,
    OpOr,
    OpNot,

    // Integer negation
    OpNegI64,
    OpNegI32,
    OpNegI16,
    OpNegI8,

    // Boolean comparisons
    OpEq,
    OpEqi,
    OpNe,
    OpNei,
    OpLtI32,
    OpLtU32,
    OpLeI32,
    OpLeU32,

    // Three-way comparisons
    OpCmpI32,
    OpCmpU32,
    OpCmpI64,
    OpCmpU64,
    OpCmplF32,
    OpCmpgF32,
    OpCmplF64,
    OpCmpgF64,

    // Branching
    OpBz,
    OpBnz,
    OpBeq,
    OpBeqi,
    OpBne,
    OpBnei,
    OpBltI32,
    OpBltU32,
    OpBleI32,
    OpBleU32,
    OpJmp,

    // Functions
    OpCall,
    OpRet,
    OpRet64,
    OpRetv,

    // Repeated instructions
    OpCallCall,
    OpLdiLdi,
    OpLdcLdc,
    OpLdgLdg,
    OpMovMov,

    // Fused calls
    OpLdiCall,
    OpLdcCall,
    OpLdgCall,
    OpLdc64Call,
    OpLdg64Call,
    OpLdrCall,
    OpStgCall,

    // Fused data movement
    OpLdiLdg,
    OpLdgLdi,
    OpMovLdi,
    OpMovLdc,
    OpMovLdg,

    // Wide prefix
    OpWide,

    OpCount
} Opcode;

OpcodeFlag get_opcode_flags(Opcode opcode);
OpcodeRegisterFlag get_opcode_register_flag(Opcode opcode);

#endif
