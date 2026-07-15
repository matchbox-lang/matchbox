#include "opcode.h"

static const OpcodeFlag opcodeFlags[OP_COUNT] = {
    [OP_MOV] = OP_FLAG_WRITES_A,
    [OP_LDI] = OP_FLAG_WRITES_A,
    [OP_LDC] = OP_FLAG_WRITES_A,
    [OP_LDG] = OP_FLAG_WRITES_A,
    [OP_ADD] = OP_FLAG_WRITES_A,
    [OP_SUB] = OP_FLAG_WRITES_A,
    [OP_MUL] = OP_FLAG_WRITES_A,
    [OP_DIV] = OP_FLAG_WRITES_A,
    [OP_REM] = OP_FLAG_WRITES_A,
    [OP_POW] = OP_FLAG_WRITES_A,
    [OP_BAND] = OP_FLAG_WRITES_A,
    [OP_BOR] = OP_FLAG_WRITES_A,
    [OP_BXOR] = OP_FLAG_WRITES_A,
    [OP_BNOT] = OP_FLAG_WRITES_A,
    [OP_LSL] = OP_FLAG_WRITES_A,
    [OP_LSR] = OP_FLAG_WRITES_A,
    [OP_ASR] = OP_FLAG_WRITES_A,
    [OP_NOT] = OP_FLAG_WRITES_A,
    [OP_NEG] = OP_FLAG_WRITES_A,
    [OP_LDI_STG] = OP_FLAG_WRITES_A
};

OpcodeFlag getOpcodeFlags(Opcode opcode)
{
    if ((unsigned)opcode >= OP_COUNT) {
        return OP_FLAG_NONE;
    }

    return opcodeFlags[opcode];
}
