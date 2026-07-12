#include "bytecode.h"
#include "code_object.h"
#include "opcode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define READ_INT8() ((int8_t)*(ptr++))
#define READ_UINT8() ((uint8_t)*(ptr++))
#define READ_INT16() (ptr += 2, (int16_t)((ptr[-2] << 8) | ptr[-1]))
#define READ_UINT16() (ptr += 2, (uint16_t)((ptr[-2] << 8) | ptr[-1]))
#define READ_INT24() (ptr += 3, (int32_t)((ptr[-3] << 24) | (ptr[-2] << 16) | (ptr[-1] << 8)) >> 8)

static uint8_t* ptr;

static void unknownOpcodeError(int opcode)
{
    fprintf(stderr, "Error: Unknown opcode %d\n", opcode);
    exit(1);
}

static void printOpcode(const char* name)
{
    ptr += 3;

    printf("%s\n", name);
}

static void printOpcodeRegister(const char* name)
{
    uint8_t a = READ_UINT8();
    ptr += 2;

    printf("%-11s R%d\n", name, a);
}

static void printOpcodeRegisters2(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    ptr++;

    printf("%-11s R%d, R%d\n", name, a, b);
}

static void printOpcodeRegisters3(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    uint8_t c = READ_UINT8();

    printf("%-11s R%d, R%d, R%d\n", name, a, b, c);
}

static void printOpcodeRegisterInt16(const char* name)
{
    uint8_t a = READ_UINT8();
    int16_t imm = READ_INT16();

    printf("%-11s R%d, %d\n", name, a, imm);
}

static void printOpcodeRegisterUint16(const char* name)
{
    uint8_t a = READ_UINT8();
    uint16_t imm = READ_UINT16();

    printf("%-11s R%d, %u\n", name, a, imm);
}

static void printOpcodeBranch(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    int8_t offset = READ_INT8();

    printf("%-11s R%d, R%d, %d\n", name, a, b, offset);
}

static void printOpcodeInt24(const char* name)
{
    int32_t imm = READ_INT24();

    printf("%-11s %d\n", name, imm);
}

static void printInstruction(int8_t c)
{
    switch (c) {
        case OP_HLT:        printOpcode("HLT"); break;
        case OP_NOP:        printOpcode("NOP"); break;
        case OP_MOV:        printOpcodeRegisters2("MOV"); break;
        case OP_LDC:        printOpcodeRegisterUint16("LDC"); break;
        case OP_LDI:        printOpcodeRegisterInt16("LDI"); break;
        case OP_REG:        printOpcodeRegister("REG"); break;
        case OP_LDG:        printOpcodeRegisterUint16("LDG"); break;
        case OP_STG:        printOpcodeRegisterUint16("STG"); break;
        case OP_ADD:        printOpcodeRegisters3("ADD"); break;
        case OP_SUB:        printOpcodeRegisters3("SUB"); break;
        case OP_MUL:        printOpcodeRegisters3("MUL"); break;
        case OP_DIV:        printOpcodeRegisters3("DIV"); break;
        case OP_REM:        printOpcodeRegisters3("REM"); break;
        case OP_POW:        printOpcodeRegisters3("POW"); break;
        case OP_BAND:       printOpcodeRegisters3("BAND"); break;
        case OP_BOR:        printOpcodeRegisters3("BOR"); break;
        case OP_BXOR:       printOpcodeRegisters3("BXOR"); break;
        case OP_BNOT:       printOpcodeRegisters2("BNOT"); break;
        case OP_LSL:        printOpcodeRegisters3("LSL"); break;
        case OP_LSR:        printOpcodeRegisters3("LSR"); break;
        case OP_ASR:        printOpcodeRegisters3("ASR"); break;
        case OP_NOT:        printOpcodeRegisters2("NOT"); break;
        case OP_NEG:        printOpcodeRegisters2("NEG"); break;
        case OP_BEQ:        printOpcodeBranch("BEQ"); break;
        case OP_BLT:        printOpcodeBranch("BLT"); break;
        case OP_BLE:        printOpcodeBranch("BLE"); break;
        case OP_JMP:        printOpcodeInt24("JMP"); break;
        case OP_CALL:       printOpcodeRegister("CALL"); break;
        case OP_RET:        printOpcode("RET"); break;
        case OP_RETV:       printOpcode("RETV"); break;
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
