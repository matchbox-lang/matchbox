#include "bytecode.h"
#include "code_object.h"
#include "opcode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define READ_INT8() ((int8_t)*(ptr++))
#define READ_INT16() (ptr += 2, (int16_t)((ptr[-2] << 8) | ptr[-1]))

static uint8_t* ptr;
static const char* opcodeError = "Error: Unknown opcode %d\n";

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
        case OP_HLT:            return printOpcode("HLT");
        case OP_LDC:            return printOpcodeInt8("LDC");
        case OP_REG:            return printOpcode("REG");
        case OP_LDG:            return printOpcodeInt8("LDG");
        case OP_STG:            return printOpcodeInt8("STG");
        case OP_LDL:            return printOpcodeInt8("LDL");
        case OP_LDL_0:          return printOpcode("LDL_0");
        case OP_LDL_1:          return printOpcode("LDL_1");
        case OP_LDL_2:          return printOpcode("LDL_2");
        case OP_LDL_3:          return printOpcode("LDL_3");
        case OP_STL:            return printOpcodeInt8("STL");
        case OP_STL_0:          return printOpcode("STL_0");
        case OP_STL_1:          return printOpcode("STL_1");
        case OP_STL_2:          return printOpcode("STL_2");
        case OP_STL_3:          return printOpcode("STL_3");
        case OP_PUSHB:          return printOpcodeInt8("PUSHB");
        case OP_PUSHH:          return printOpcodeInt16("PUSHH");
        case OP_PUSH_0:         return printOpcode("PUSH_0");
        case OP_PUSH_1:         return printOpcode("PUSH_1");
        case OP_PUSH_2:         return printOpcode("PUSH_2");
        case OP_PUSH_3:         return printOpcode("PUSH_3");
        case OP_POP:            return printOpcode("POP");
        case OP_DUP:            return printOpcode("DUP");
        case OP_INC:            return printOpcode("INC");
        case OP_DEC:            return printOpcode("DEC");
        case OP_ADD:            return printOpcode("ADD");
        case OP_SUB:            return printOpcode("SUB");
        case OP_MUL:            return printOpcode("MUL");
        case OP_DIV:            return printOpcode("DIV");
        case OP_REM:            return printOpcode("REM");
        case OP_POW:            return printOpcode("POW");
        case OP_BAND:           return printOpcode("BAND");
        case OP_BOR:            return printOpcode("BOR");
        case OP_BXOR:           return printOpcode("BXOR");
        case OP_BNOT:           return printOpcode("BNOT");
        case OP_LSL:            return printOpcode("LSL");
        case OP_LSR:            return printOpcode("LSR");
        case OP_ASR:            return printOpcode("ASR");
        case OP_NOT:            return printOpcode("NOT");
        case OP_NEG:            return printOpcode("NEG");
        case OP_BEQ:            return printOpcodeInt16("BEQ");
        case OP_BLT:            return printOpcodeInt16("BLT");
        case OP_BLE:            return printOpcodeInt16("BLE");
        case OP_JMP:            return printOpcodeInt16("JMP");
        case OP_CALLBI:         return printOpcodeInt8("CALLBI");
        case OP_CALL:           return printOpcodeInt16("CALL");
        case OP_RET:            return printOpcode("RET");
        case OP_RETV:           return printOpcode("RETV");
        default:
            fprintf(stderr, opcodeError, c);
            exit(1);
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
