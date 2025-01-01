#include "bytecode.h"
#include <stdio.h>

#define READ_INT16() (ptr += 2, (int16_t)((ptr[-2] << 8) | ptr[-1]))
#define READ_INT8() ((int8_t)*(ptr++))

static uint8_t* ptr;

static int printInstruction(int8_t c)
{
    switch (c) {
        case OP_HLT:        return printf("hlt\n");
        case OP_SYSCALL:    return printf("syscall\t\t%d\n", READ_INT8());
        case OP_LDC:        return printf("ldc\t\t%d\n", READ_INT8());
        case OP_LDG:        return printf("ldg\t\t%d\n", READ_INT8());
        case OP_STG:        return printf("stg\t\t%d\n", READ_INT8());
        case OP_LDL:        return printf("ldl\t\t%d\n", READ_INT8());
        case OP_LDL_0:      return printf("ldl_0\n");
        case OP_LDL_1:      return printf("ldl_1\n");
        case OP_LDL_2:      return printf("ldl_2\n");
        case OP_LDL_3:      return printf("ldl_3\n");
        case OP_STL:        return printf("stl\t\t%d\n", READ_INT8());
        case OP_STL_0:      return printf("stl_0\n");
        case OP_STL_1:      return printf("stl_1\n");
        case OP_STL_2:      return printf("stl_2\n");
        case OP_STL_3:      return printf("stl_3\n");
        case OP_PUSHB:      return printf("push\t\t%d\n", READ_INT8());
        case OP_PUSHH:      return printf("push\t\t%d\n", READ_INT16());
        case OP_PUSH_0:     return printf("push_0\n");
        case OP_PUSH_1:     return printf("push_1\n");
        case OP_PUSH_2:     return printf("push_2\n");
        case OP_PUSH_3:     return printf("push_3\n");
        case OP_POP:        return printf("pop\n");
        case OP_DUP:        return printf("dup\n");
        case OP_INC:        return printf("inc\n");
        case OP_DEC:        return printf("dec\n");
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
        case OP_NOT:        return printf("not\n");
        case OP_NEG:        return printf("neg\n");
        case OP_BEQ:        return printf("beq\t\t%d\n", READ_INT16());
        case OP_BLT:        return printf("blt\t\t%d\n", READ_INT16());
        case OP_BLE:        return printf("ble\t\t%d\n", READ_INT16());
        case OP_JMP:        return printf("jmp\t\t%d\n", READ_INT16());
        case OP_CALL:       return printf("call\t\t%d\n", READ_INT16());
        case OP_RES:        return printf("res\t\t%d\n", READ_INT8());
        case OP_RET:        return printf("ret\n");
        case OP_RETV:       return printf("retv\n");
    }
}

void disassemble(Chunk* chunk)
{
    ptr = chunk->data;

    while (ptr != chunkEnd(chunk)) {
        int8_t c = READ_INT8();

        printInstruction(c);
    }
}
