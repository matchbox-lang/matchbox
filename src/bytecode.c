#include "bytecode.h"
#include "code_object.h"
#include "opcode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define READ_INT8() ((int8_t)*(ptr++))
#define READ_INT16() (ptr += 2, (int16_t)((ptr[-2] << 8) | ptr[-1]))

static uint8_t* ptr;

static void unknownOpcodeError(int opcode)
{
    fprintf(stderr, "Error: Unknown opcode %d\n", opcode);
    exit(1);
}

static int printOpcode(const char* name)
{
    return printf("%s\n", name);
}

static int printOpcodeInt8(const char* name)
{
    return printf("%-11s %d\n", name, READ_INT8());
}

static int printOpcodeInt16(const char* name)
{
    return printf("%-11s %d\n", name, READ_INT16());
}

static int printInstruction(int8_t c)
{
    switch (c) {
        case OP_HLT:            return printOpcode("hlt");
        case OP_LDC:            return printOpcodeInt8("ldc");
        case OP_REG:            return printOpcode("reg");
        case OP_LDG:            return printOpcodeInt8("ldg");
        case OP_STG:            return printOpcodeInt8("stg");
        case OP_LDL:            return printOpcodeInt8("ldl");
        case OP_LDL_0:          return printOpcode("ldl_0");
        case OP_LDL_1:          return printOpcode("ldl_1");
        case OP_LDL_2:          return printOpcode("ldl_2");
        case OP_LDL_3:          return printOpcode("ldl_3");
        case OP_STL:            return printOpcodeInt8("stl");
        case OP_STL_0:          return printOpcode("stl_0");
        case OP_STL_1:          return printOpcode("stl_1");
        case OP_STL_2:          return printOpcode("stl_2");
        case OP_STL_3:          return printOpcode("stl_3");
        case OP_PUSHB:          return printOpcodeInt8("pushb");
        case OP_PUSHH:          return printOpcodeInt16("pushh");
        case OP_PUSH_0:         return printOpcode("push_0");
        case OP_PUSH_1:         return printOpcode("push_1");
        case OP_PUSH_2:         return printOpcode("push_2");
        case OP_PUSH_3:         return printOpcode("push_3");
        case OP_POP:            return printOpcode("pop");
        case OP_DUP:            return printOpcode("dup");
        case OP_INC:            return printOpcode("inc");
        case OP_DEC:            return printOpcode("dec");
        case OP_ADD:            return printOpcode("add");
        case OP_SUB:            return printOpcode("sub");
        case OP_MUL:            return printOpcode("mul");
        case OP_DIV:            return printOpcode("div");
        case OP_REM:            return printOpcode("rem");
        case OP_POW:            return printOpcode("pow");
        case OP_BAND:           return printOpcode("band");
        case OP_BOR:            return printOpcode("bor");
        case OP_BXOR:           return printOpcode("bxor");
        case OP_BNOT:           return printOpcode("bnot");
        case OP_LSL:            return printOpcode("lsl");
        case OP_LSR:            return printOpcode("lsr");
        case OP_ASR:            return printOpcode("asr");
        case OP_NOT:            return printOpcode("not");
        case OP_NEG:            return printOpcode("neg");
        case OP_BEQ:            return printOpcodeInt16("beq");
        case OP_BLT:            return printOpcodeInt16("blt");
        case OP_BLE:            return printOpcodeInt16("ble");
        case OP_JMP:            return printOpcodeInt16("jmp");
        case OP_CALLBI:         return printOpcodeInt8("callbi");
        case OP_CALL:           return printOpcodeInt16("call");
        case OP_RET:            return printOpcode("ret");
        case OP_RETV:           return printOpcode("retv");
        default:
            unknownOpcodeError(c);
    }
}

void disassemble(CodeObject* code)
{
    ptr = code->data;

    while (ptr != codeObjectEnd(code)) {
        int8_t c = READ_INT8();

        printInstruction(c);
    }
}
