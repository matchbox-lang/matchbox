#ifndef BYTECODE_H
#define BYTECODE_H

#include "chunk.h"

typedef enum Opcode
{
    OP_HLT,         // hlt
    OP_SYSCALL,     // syscall
    OP_LDC,         // ldc imm8
    OP_LDL,         // ldl imm8
    OP_LDL_0,       // ldl_0
    OP_LDL_1,       // ldl_1
    OP_LDL_2,       // ldl_2
    OP_STL,         // stl imm8
    OP_STL_0,       // stl_0
    OP_STL_1,       // stl_1
    OP_STL_2,       // stl_2
    OP_LDN,         // ldn imm8, imm8
    OP_STN,         // stn imm8, imm8
    OP_PUSH,        // push imm8
    OP_PUSH_0,      // push_0
    OP_PUSH_1,      // push_1
    OP_PUSH_2,      // push_2
    OP_POP,         // pop
    OP_INC,         // inc imm8
    OP_DEC,         // dec imm8
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
