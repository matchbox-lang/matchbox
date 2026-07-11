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

static void printOpcode(const char* name)
{
    printf("%s\n", name);
}

static void printOpcodeInt8(const char* name)
{
    printf("%-11s %d\n", name, READ_INT8());
}

static void printOpcodeInt16(const char* name)
{
    printf("%-11s %d\n", name, READ_INT16());
}

static void printInstruction(int8_t c)
{
    switch (c) {
        case OP_HLT:        printOpcode("hlt"); break;
        case OP_LDC:        printOpcodeInt8("ldc"); break;
        case OP_REG:        printOpcode("reg"); break;
        case OP_LDG:        printOpcodeInt8("ldg"); break;
        case OP_STG:        printOpcodeInt8("stg"); break;
        case OP_LDL:        printOpcodeInt8("ldl"); break;
        case OP_LDL_0:      printOpcode("ldl_0"); break;
        case OP_LDL_1:      printOpcode("ldl_1"); break;
        case OP_LDL_2:      printOpcode("ldl_2"); break;
        case OP_LDL_3:      printOpcode("ldl_3"); break;
        case OP_STL:        printOpcodeInt8("stl"); break;
        case OP_STL_0:      printOpcode("stl_0"); break;
        case OP_STL_1:      printOpcode("stl_1"); break;
        case OP_STL_2:      printOpcode("stl_2"); break;
        case OP_STL_3:      printOpcode("stl_3"); break;
        case OP_PUSHB:      printOpcodeInt8("pushb"); break;
        case OP_PUSHH:      printOpcodeInt16("pushh"); break;
        case OP_PUSH_0:     printOpcode("push_0"); break;
        case OP_PUSH_1:     printOpcode("push_1"); break;
        case OP_PUSH_2:     printOpcode("push_2"); break;
        case OP_PUSH_3:     printOpcode("push_3"); break;
        case OP_POP:        printOpcode("pop"); break;
        case OP_DUP:        printOpcode("dup"); break;
        case OP_INC:        printOpcode("inc"); break;
        case OP_DEC:        printOpcode("dec"); break;
        case OP_ADD:        printOpcode("add"); break;
        case OP_SUB:        printOpcode("sub"); break;
        case OP_MUL:        printOpcode("mul"); break;
        case OP_DIV:        printOpcode("div"); break;
        case OP_REM:        printOpcode("rem"); break;
        case OP_POW:        printOpcode("pow"); break;
        case OP_BAND:       printOpcode("band"); break;
        case OP_BOR:        printOpcode("bor"); break;
        case OP_BXOR:       printOpcode("bxor"); break;
        case OP_BNOT:       printOpcode("bnot"); break;
        case OP_LSL:        printOpcode("lsl"); break;
        case OP_LSR:        printOpcode("lsr"); break;
        case OP_ASR:        printOpcode("asr"); break;
        case OP_NOT:        printOpcode("not"); break;
        case OP_NEG:        printOpcode("neg"); break;
        case OP_BEQ:        printOpcodeInt16("beq"); break;
        case OP_BLT:        printOpcodeInt16("blt"); break;
        case OP_BLE:        printOpcodeInt16("ble"); break;
        case OP_JMP:        printOpcodeInt16("jmp"); break;
        case OP_CALL:       printOpcodeInt16("call"); break;
        case OP_RET:        printOpcode("ret"); break;
        case OP_RETV:       printOpcode("retv"); break;
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
