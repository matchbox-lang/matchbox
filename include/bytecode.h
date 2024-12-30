#ifndef BYTECODE_H
#define BYTECODE_H

#include "chunk.h"

typedef enum Opcode
{
    OP_HLT,         // hlt
    OP_SYSCALL,     // syscall imm8
    OP_LDC,         // ldc imm8
    OP_LDG,         // ldg imm8
    OP_STG,         // stg imm8
    OP_LDL,         // ldl imm8
    OP_LDL_0,       // ldl 0
    OP_LDL_1,       // ldl 1
    OP_LDL_2,       // ldl 2
    OP_LDL_3,       // ldl 3
    OP_STL,         // stl imm8
    OP_STL_0,       // stl 0
    OP_STL_1,       // stl 1
    OP_STL_2,       // stl 2
    OP_STL_3,       // stl 3
    OP_PUSHB,       // push imm8
    OP_PUSHH,       // push imm16
    OP_PUSH_N1,     // push -1
    OP_PUSH_0,      // push 0
    OP_PUSH_1,      // push 1
    OP_PUSH_2,      // push 2
    OP_PUSH_3,      // push 3
    OP_POP,         // pop
    OP_DUP,         // dup
    OP_INC,         // inc
    OP_DEC,         // dec
    OP_ADD,         // add
    OP_SUB,         // sub
    OP_MUL,         // mul
    OP_DIV,         // div
    OP_REM,         // rem
    OP_POW,         // pow
    OP_BAND,        // band
    OP_BOR,         // bor
    OP_BXOR,        // bxor
    OP_BNOT,        // bnot
    OP_LSL,         // lsl
    OP_LSR,         // lsr
    OP_ASR,         // asr
    OP_NOT,         // not
    OP_NEG,         // neg
    OP_BEQ,         // beq imm16
    OP_BLT,         // blt imm16
    OP_BLE,         // ble imm16
    OP_JMP,         // jmp imm16
    OP_CALL,        // call imm16
    OP_RET,         // ret
    OP_RETV         // retv
} Opcode;

void disassemble(Chunk* chunk);

#endif
