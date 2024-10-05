#include "bytecode.h"
#include <stdio.h>

#define READ_UINT16() (ptr += 2, (int16_t)((ptr[-2] << 8) | ptr[-1]))
#define READ_UINT8() ((int8_t)*(ptr++))

uint8_t* ptr;

static int printInstruction(int8_t c)
{
    switch (c) {
        case OP_HLT:        return printf("hlt\n");
        case OP_SYSCALL:    return printf("syscall\n");
        case OP_LDC:        return printf("ldc\t%d\n", READ_UINT8());
        case OP_LDL:        return printf("ldl\t%d\n", READ_UINT8());
        case OP_LDL_0:      return printf("ldl_0\n");
        case OP_LDL_1:      return printf("ldl_1\n");
        case OP_LDL_2:      return printf("ldl_2\n");
        case OP_STL:        return printf("stl\t%d\n", READ_UINT8());
        case OP_STL_0:      return printf("stl_0\n");
        case OP_STL_1:      return printf("stl_1\n");
        case OP_STL_2:      return printf("stl_2\n");
        case OP_PUSH:       return printf("push\t%d\n", READ_UINT8());
        case OP_PUSH_0:     return printf("push_0\n");
        case OP_PUSH_1:     return printf("push_1\n");
        case OP_POP:        return printf("pop\n");
        case OP_ADD:        return printf("add\n");
        case OP_SUB:        return printf("sub\n");
        case OP_MUL:        return printf("mul\n");
        case OP_DIV:        return printf("div\n");
        case OP_REM:        return printf("rem\n");
        case OP_POW:        return printf("pow\n");
        case OP_BAND:       return printf("band\n");
        case OP_BOR:        return printf("bor\n");
        case OP_BXOR:       return printf("bxor\n");
        case OP_BNOT:       return printf("bnot\n");
        case OP_LSL:        return printf("lsl\n");
        case OP_LSR:        return printf("lsr\n");
        case OP_ASR:        return printf("asr\n");
        case OP_ABS:        return printf("abs\n");
        case OP_NOT:        return printf("not\n");
        case OP_NEG:        return printf("neg\n");
        case OP_INC:        return printf("inc\n");
        case OP_DEC:        return printf("dec\n");
        case OP_BEQ:        return printf("beq\t%d\n", READ_UINT16());
        case OP_BLT:        return printf("blt\t%d\n", READ_UINT16());
        case OP_BLE:        return printf("ble\t%d\n", READ_UINT16());
        case OP_JMP:        return printf("jmp\t%d\n", READ_UINT16());
        case OP_JSR:        return printf("jsr\t%d\n", READ_UINT16());
        case OP_RET:        return printf("ret\n");
        case OP_RETV:       return printf("retv\n");
    }
}

void disassemble(Chunk* chunk)
{
    ptr = chunk->data;

    while (ptr != &chunk->data[chunk->count]) {
        int8_t c = READ_UINT8();

        printInstruction(c);
    }
}