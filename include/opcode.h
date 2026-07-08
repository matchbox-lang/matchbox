#ifndef OPCODE_H
#define OPCODE_H

typedef enum Opcode
{
    OP_HLT,         	// HLT
    OP_LDC,         	// LDC imm8
    OP_REG,         	// REG
    OP_LDG,         	// LDG imm8
    OP_STG,         	// STG imm8
    OP_LDL,         	// LDL imm8
    OP_LDL_0,       	// LDL_0
    OP_LDL_1,       	// LDL_1
    OP_LDL_2,       	// LDL_2
    OP_LDL_3,       	// LDL_3
    OP_STL,         	// STL imm8
    OP_STL_0,       	// STL_0
    OP_STL_1,       	// STL_1
    OP_STL_2,       	// STL_2
    OP_STL_3,       	// STL_3
    OP_PUSHB,       	// PUSHB imm8
    OP_PUSHH,       	// PUSHH imm16
    OP_PUSH_0,      	// PUSH_0
    OP_PUSH_1,      	// PUSH_1
    OP_PUSH_2,      	// PUSH_2
    OP_PUSH_3,      	// PUSH_3
    OP_POP,         	// POP
    OP_DUP,         	// DUP
    OP_INC,         	// INC
    OP_DEC,         	// DEC
    OP_ADD,         	// ADD
    OP_SUB,         	// SUB
    OP_MUL,         	// MUL
    OP_DIV,         	// DIV
    OP_REM,         	// REM
    OP_POW,         	// POW
    OP_BAND,        	// BAND
    OP_BOR,         	// BOR
    OP_BXOR,        	// BXOR
    OP_BNOT,        	// BNOT
    OP_LSL,         	// LSL
    OP_LSR,         	// LSR
    OP_ASR,         	// ASR
    OP_NOT,         	// NOT
    OP_NEG,         	// NEG
    OP_BEQ,         	// BEQ imm16
    OP_BLT,         	// BLT imm16
    OP_BLE,         	// BLE imm16
    OP_JMP,         	// JMP imm16
    OP_CALLBI,          // CALLBI imm8
    OP_CALL,            // CALL imm16
    OP_RET,         	// RET
    OP_RETV         	// RETV
} Opcode;

#endif
