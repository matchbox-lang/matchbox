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

    printf("%-15s R%d\n", name, a);
}

static void printOpcodeRegisters2(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    ptr++;

    printf("%-15s R%d, R%d\n", name, a, b);
}

static void printOpcodeRegisters3(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    uint8_t c = READ_UINT8();

    printf("%-15s R%d, R%d, R%d\n", name, a, b, c);
}

static void printOpcodeRegisters2Int8(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    int8_t imm = READ_INT8();

    printf("%-15s R%d, R%d, #%d\n", name, a, b, imm);
}

static void printOpcodeRegisterInt16(const char* name)
{
    uint8_t a = READ_UINT8();
    int16_t imm = READ_INT16();

    printf("%-15s R%d, #%d\n", name, a, imm);
}

static void printOpcodeRegisterUint16(const char* name)
{
    uint8_t a = READ_UINT8();
    uint16_t imm = READ_UINT16();

    printf("%-15s R%d, #%u\n", name, a, imm);
}

static void printOpcodeRegisterInt8Uint8(const char* name)
{
    uint8_t a = READ_UINT8();
    int8_t imm = READ_INT8();
    uint8_t position = READ_UINT8();

    printf("%-15s R%d, #%d, #%u\n", name, a, imm, position);
}

static void printOpcodeRegisterUint8Int8(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t position = READ_UINT8();
    int8_t imm = READ_INT8();

    printf("%-15s R%d, #%u, #%d\n", name, a, position, imm);
}

static void printOpcodeRegisterInt8Int8(const char* name)
{
    uint8_t a = READ_UINT8();
    int8_t first = READ_INT8();
    int8_t second = READ_INT8();

    printf("%-15s R%d, #%d, #%d\n", name, a, first, second);
}

static void printOpcodeRegisterUint8Uint8(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t first = READ_UINT8();
    uint8_t second = READ_UINT8();

    printf("%-15s R%d, #%u, #%u\n", name, a, first, second);
}

static void printLoadCall(const char* name)
{
    uint8_t a = READ_UINT8();
    uint16_t operands = READ_UINT16();

    printf("%-15s R%d, #%u, #%u\n", name, a,
        LOAD_CALL_OPERAND(operands), LOAD_CALL_FUNCTION(operands));
}

static void printSignedLoadCall(const char* name)
{
    uint8_t a = READ_UINT8();
    uint16_t operands = READ_UINT16();

    printf("%-15s R%d, #%d, #%u\n", name, a, LOAD_CALL_SIGNED_OPERAND(operands), LOAD_CALL_FUNCTION(operands));
}

static void printOpcodeBranch(const char* name)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    int8_t offset = READ_INT8();

    printf("%-15s R%d, R%d, #%d\n", name, a, b, offset);
}

static void printOpcodeInt24(const char* name)
{
    int32_t imm = READ_INT24();

    printf("%-15s #%d\n", name, imm);
}

static void printInstruction(uint8_t c)
{
    switch (c) {
        case OP_HLT:            printOpcode("HLT"); break;
        case OP_NOP:            printOpcode("NOP"); break;
        case OP_MOV:            printOpcodeRegisters2("MOV"); break;
        case OP_LDI:            printOpcodeRegisterInt16("LDI"); break;
        case OP_LDC:            printOpcodeRegisterUint16("LDC"); break;
        case OP_LDG:            printOpcodeRegisterUint16("LDG"); break;
        case OP_STG:            printOpcodeRegisterUint16("STG"); break;
        case OP_REFL:           printOpcodeRegisters2("REFL"); break;
        case OP_REFG:           printOpcodeRegisterUint16("REFG"); break;
        case OP_LDR:            printOpcodeRegisters2("LDR"); break;
        case OP_STR:            printOpcodeRegisters2("STR"); break;
        case OP_MOV_I64:        printOpcodeRegisters2("MOV_I64"); break;
        case OP_LDC_I64:        printOpcodeRegisterUint16("LDC_I64"); break;
        case OP_LDG_I64:        printOpcodeRegisterUint16("LDG_I64"); break;
        case OP_STG_I64:        printOpcodeRegisterUint16("STG_I64"); break;
        case OP_LDR_I64:        printOpcodeRegisters2("LDR_I64"); break;
        case OP_STR_I64:        printOpcodeRegisters2("STR_I64"); break;
        case OP_ADD_I64:        printOpcodeRegisters3("ADD_I64"); break;
        case OP_ADD_I32:        printOpcodeRegisters3("ADD_I32"); break;
        case OP_ADD_I16:        printOpcodeRegisters3("ADD_I16"); break;
        case OP_ADD_I8:         printOpcodeRegisters3("ADD_I8"); break;
        case OP_ADDI_I64:       printOpcodeRegisters2Int8("ADDI_I64"); break;
        case OP_ADDI_I32:       printOpcodeRegisters2Int8("ADDI_I32"); break;
        case OP_ADDI_I16:       printOpcodeRegisters2Int8("ADDI_I16"); break;
        case OP_ADDI_I8:        printOpcodeRegisters2Int8("ADDI_I8"); break;
        case OP_SUB_I64:        printOpcodeRegisters3("SUB_I64"); break;
        case OP_SUB_I32:        printOpcodeRegisters3("SUB_I32"); break;
        case OP_SUB_I16:        printOpcodeRegisters3("SUB_I16"); break;
        case OP_SUB_I8:         printOpcodeRegisters3("SUB_I8"); break;
        case OP_SUBI_I64:       printOpcodeRegisters2Int8("SUBI_I64"); break;
        case OP_SUBI_I32:       printOpcodeRegisters2Int8("SUBI_I32"); break;
        case OP_SUBI_I16:       printOpcodeRegisters2Int8("SUBI_I16"); break;
        case OP_SUBI_I8:        printOpcodeRegisters2Int8("SUBI_I8"); break;
        case OP_MUL_I64:        printOpcodeRegisters3("MUL_I64"); break;
        case OP_MUL_I32:        printOpcodeRegisters3("MUL_I32"); break;
        case OP_MUL_I16:        printOpcodeRegisters3("MUL_I16"); break;
        case OP_MUL_I8:         printOpcodeRegisters3("MUL_I8"); break;
        case OP_IDIV_I64:       printOpcodeRegisters3("IDIV_I64"); break;
        case OP_IDIV_U64:       printOpcodeRegisters3("IDIV_U64"); break;
        case OP_IDIV_I32:       printOpcodeRegisters3("IDIV_I32"); break;
        case OP_IDIV_U32:       printOpcodeRegisters3("IDIV_U32"); break;
        case OP_IDIV_I16:       printOpcodeRegisters3("IDIV_I16"); break;
        case OP_IDIV_U16:       printOpcodeRegisters3("IDIV_U16"); break;
        case OP_IDIV_I8:        printOpcodeRegisters3("IDIV_I8"); break;
        case OP_IDIV_U8:        printOpcodeRegisters3("IDIV_U8"); break;
        case OP_REM_I64:        printOpcodeRegisters3("REM_I64"); break;
        case OP_REM_U64:        printOpcodeRegisters3("REM_U64"); break;
        case OP_REM_I32:        printOpcodeRegisters3("REM_I32"); break;
        case OP_REM_U32:        printOpcodeRegisters3("REM_U32"); break;
        case OP_REM_I16:        printOpcodeRegisters3("REM_I16"); break;
        case OP_REM_U16:        printOpcodeRegisters3("REM_U16"); break;
        case OP_REM_I8:         printOpcodeRegisters3("REM_I8"); break;
        case OP_REM_U8:         printOpcodeRegisters3("REM_U8"); break;
        case OP_BAND_I64:       printOpcodeRegisters3("BAND_I64"); break;
        case OP_BAND_I32:       printOpcodeRegisters3("BAND_I32"); break;
        case OP_BAND_I16:       printOpcodeRegisters3("BAND_I16"); break;
        case OP_BAND_I8:        printOpcodeRegisters3("BAND_I8"); break;
        case OP_BANDI_I64:      printOpcodeRegisters2Int8("BANDI_I64"); break;
        case OP_BANDI_I32:      printOpcodeRegisters2Int8("BANDI_I32"); break;
        case OP_BANDI_I16:      printOpcodeRegisters2Int8("BANDI_I16"); break;
        case OP_BANDI_I8:       printOpcodeRegisters2Int8("BANDI_I8"); break;
        case OP_BOR_I64:        printOpcodeRegisters3("BOR_I64"); break;
        case OP_BOR_I32:        printOpcodeRegisters3("BOR_I32"); break;
        case OP_BOR_I16:        printOpcodeRegisters3("BOR_I16"); break;
        case OP_BOR_I8:         printOpcodeRegisters3("BOR_I8"); break;
        case OP_BORI_I64:       printOpcodeRegisters2Int8("BORI_I64"); break;
        case OP_BORI_I32:       printOpcodeRegisters2Int8("BORI_I32"); break;
        case OP_BORI_I16:       printOpcodeRegisters2Int8("BORI_I16"); break;
        case OP_BORI_I8:        printOpcodeRegisters2Int8("BORI_I8"); break;
        case OP_BXOR_I64:       printOpcodeRegisters3("BXOR_I64"); break;
        case OP_BXOR_I32:       printOpcodeRegisters3("BXOR_I32"); break;
        case OP_BXOR_I16:       printOpcodeRegisters3("BXOR_I16"); break;
        case OP_BXOR_I8:        printOpcodeRegisters3("BXOR_I8"); break;
        case OP_BXORI_I64:      printOpcodeRegisters2Int8("BXORI_I64"); break;
        case OP_BXORI_I32:      printOpcodeRegisters2Int8("BXORI_I32"); break;
        case OP_BXORI_I16:      printOpcodeRegisters2Int8("BXORI_I16"); break;
        case OP_BXORI_I8:       printOpcodeRegisters2Int8("BXORI_I8"); break;
        case OP_BNOT_I64:       printOpcodeRegisters2("BNOT_I64"); break;
        case OP_BNOT_I32:       printOpcodeRegisters2("BNOT_I32"); break;
        case OP_BNOT_I16:       printOpcodeRegisters2("BNOT_I16"); break;
        case OP_BNOT_I8:        printOpcodeRegisters2("BNOT_I8"); break;
        case OP_LSL_I64:        printOpcodeRegisters3("LSL_I64"); break;
        case OP_LSL_I32:        printOpcodeRegisters3("LSL_I32"); break;
        case OP_LSL_I16:        printOpcodeRegisters3("LSL_I16"); break;
        case OP_LSL_I8:         printOpcodeRegisters3("LSL_I8"); break;
        case OP_LSLI_I64:       printOpcodeRegisters2Int8("LSLI_I64"); break;
        case OP_LSLI_I32:       printOpcodeRegisters2Int8("LSLI_I32"); break;
        case OP_LSLI_I16:       printOpcodeRegisters2Int8("LSLI_I16"); break;
        case OP_LSLI_I8:        printOpcodeRegisters2Int8("LSLI_I8"); break;
        case OP_LSR_I64:        printOpcodeRegisters3("LSR_I64"); break;
        case OP_LSR_I32:        printOpcodeRegisters3("LSR_I32"); break;
        case OP_LSR_I16:        printOpcodeRegisters3("LSR_I16"); break;
        case OP_LSR_I8:         printOpcodeRegisters3("LSR_I8"); break;
        case OP_LSRI_I64:       printOpcodeRegisters2Int8("LSRI_I64"); break;
        case OP_LSRI_I32:       printOpcodeRegisters2Int8("LSRI_I32"); break;
        case OP_LSRI_I16:       printOpcodeRegisters2Int8("LSRI_I16"); break;
        case OP_LSRI_I8:        printOpcodeRegisters2Int8("LSRI_I8"); break;
        case OP_ASR_I64:        printOpcodeRegisters3("ASR_I64"); break;
        case OP_ASR_I32:        printOpcodeRegisters3("ASR_I32"); break;
        case OP_ASR_I16:        printOpcodeRegisters3("ASR_I16"); break;
        case OP_ASR_I8:         printOpcodeRegisters3("ASR_I8"); break;
        case OP_ASRI_I64:       printOpcodeRegisters2Int8("ASRI_I64"); break;
        case OP_ASRI_I32:       printOpcodeRegisters2Int8("ASRI_I32"); break;
        case OP_ASRI_I16:       printOpcodeRegisters2Int8("ASRI_I16"); break;
        case OP_ASRI_I8:        printOpcodeRegisters2Int8("ASRI_I8"); break;
        case OP_AND:            printOpcodeRegisters3("AND"); break;
        case OP_OR:             printOpcodeRegisters3("OR"); break;
        case OP_NOT:            printOpcodeRegisters2("NOT"); break;
        case OP_NEG:            printOpcodeRegisters2("NEG"); break;
        case OP_NEG_I64:        printOpcodeRegisters2("NEG_I64"); break;
        case OP_EQ:             printOpcodeRegisters3("EQ"); break;
        case OP_NE:             printOpcodeRegisters3("NE"); break;
        case OP_LT_INT:         printOpcodeRegisters3("LT_INT"); break;
        case OP_LT_UINT:        printOpcodeRegisters3("LT_UINT"); break;
        case OP_LE_INT:         printOpcodeRegisters3("LE_INT"); break;
        case OP_LE_UINT:        printOpcodeRegisters3("LE_UINT"); break;
        case OP_BZ:             printOpcodeRegisterInt16("BZ"); break;
        case OP_BNZ:            printOpcodeRegisterInt16("BNZ"); break;
        case OP_BEQ:            printOpcodeBranch("BEQ"); break;
        case OP_BNE:            printOpcodeBranch("BNE"); break;
        case OP_BLT_INT:        printOpcodeBranch("BLT_INT"); break;
        case OP_BLT_UINT:       printOpcodeBranch("BLT_UINT"); break;
        case OP_BLE_INT:        printOpcodeBranch("BLE_INT"); break;
        case OP_BLE_UINT:       printOpcodeBranch("BLE_UINT"); break;
        case OP_JMP:            printOpcodeInt24("JMP"); break;
        case OP_CALL:           printOpcodeRegisterUint16("CALL"); break;
        case OP_RET:            printOpcode("RET"); break;
        case OP_RET_I64:        printOpcodeRegister("RET_I64"); break;
        case OP_RETV:           printOpcodeRegister("RETV"); break;
        case OP_CALL2:          printOpcodeRegisterUint8Uint8("CALL2"); break;
        case OP_LDI2:           printOpcodeRegisterInt8Int8("LDI2"); break;
        case OP_LDG2:           printOpcodeRegisterUint8Uint8("LDG2"); break;
        case OP_MOV2:           printOpcodeRegisters3("MOV2"); break;
        case OP_LDI_CALL:       printSignedLoadCall("LDI_CALL"); break;
        case OP_LDC_CALL:       printLoadCall("LDC_CALL"); break;
        case OP_LDG_CALL:       printLoadCall("LDG_CALL"); break;
        case OP_LDR_CALL:       printLoadCall("LDR_CALL"); break;
        case OP_LDI_STG:        printOpcodeRegisterInt8Uint8("LDI_STG"); break;
        case OP_LDI_LDG:        printOpcodeRegisterInt8Uint8("LDI_LDG"); break;
        case OP_LDG_LDI:        printOpcodeRegisterUint8Int8("LDG_LDI"); break;
        default:
            unknownOpcodeError(c);
    }
}


void disassemble(CodeObject* code)
{
    ptr = code->data;

    while (ptr != codeObjectEnd(code)) {
        uint8_t c = READ_UINT8();

        printInstruction(c);
    }
}
