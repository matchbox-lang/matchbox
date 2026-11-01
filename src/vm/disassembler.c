#include "vm/disassembler.h"
#include "common/errors.h"
#include "runtime/code_object.h"
#include "runtime/function_object.h"
#include "vm/opcode.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define READ_INT8() ((int8_t)*(ptr++))
#define READ_UINT8() ((uint8_t)*(ptr++))
#define READ_INT16() (ptr += 2, (int16_t)((ptr[-2] << 8) | ptr[-1]))
#define READ_UINT16() (ptr += 2, (uint16_t)((ptr[-2] << 8) | ptr[-1]))
#define READ_INT24() (ptr += 3, (int32_t)((ptr[-3] << 24) | (ptr[-2] << 16) | (ptr[-1] << 8)) >> 8)

static const uint8_t* ptr;
static uint16_t register_high[3];

static _Noreturn void unknown_opcode_error(int opcode)
{
    fatal_error("Unknown opcode %d", opcode);
}

static uint16_t read_register(int field)
{
    uint8_t low = READ_UINT8();

    return register_high[field] | low;
}

static void print_wide_prefix(void)
{
    uint8_t a = READ_UINT8();
    uint8_t b = READ_UINT8();
    uint8_t c = READ_UINT8();
    
    register_high[0] = (uint16_t)a << 8;
    register_high[1] = (uint16_t)b << 8;
    register_high[2] = (uint16_t)c << 8;

    printf("%-15s #%u, #%u, #%u\n", "WIDE", a, b, c);
}

static void print_opcode(const char* name)
{
    ptr += 3;

    printf("%s\n", name);
}

static void print_opcode_register(const char* name)
{
    uint16_t a = read_register(0);
    ptr += 2;

    printf("%-15s R%d\n", name, a);
}

static void print_opcode_registers2(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t b = read_register(1);
    ptr++;

    printf("%-15s R%d, R%d\n", name, a, b);
}

static void print_opcode_registers3(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t b = read_register(1);
    uint16_t c = read_register(2);

    printf("%-15s R%d, R%d, R%d\n", name, a, b, c);
}

static void print_opcode_registers2_int8(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t b = read_register(1);
    int8_t imm = READ_INT8();

    printf("%-15s R%d, R%d, #%d\n", name, a, b, imm);
}

static void print_opcode_registers2_uint8(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t b = read_register(1);
    uint8_t index = READ_UINT8();

    printf("%-15s R%d, R%d, #%u\n", name, a, b, index);
}

static void print_opcode_register_int16(const char* name)
{
    uint16_t a = read_register(0);
    int16_t imm = READ_INT16();

    printf("%-15s R%d, #%d\n", name, a, imm);
}

static void print_opcode_register_uint16(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t imm = READ_UINT16();

    printf("%-15s R%d, #%u\n", name, a, imm);
}

static void print_opcode_register_int8_uint8(const char* name)
{
    uint16_t a = read_register(0);
    int8_t imm = READ_INT8();
    uint8_t position = READ_UINT8();

    printf("%-15s R%d, #%d, #%u\n", name, a, imm, position);
}

static void print_opcode_register_uint8_int8(const char* name)
{
    uint16_t a = read_register(0);
    uint8_t position = READ_UINT8();
    int8_t imm = READ_INT8();

    printf("%-15s R%d, #%u, #%d\n", name, a, position, imm);
}

static void print_opcode_register_int8_int8(const char* name)
{
    uint16_t a = read_register(0);
    int8_t first = READ_INT8();
    int8_t second = READ_INT8();

    printf("%-15s R%d, #%d, #%d\n", name, a, first, second);
}

static void print_opcode_register_uint8_uint8(const char* name)
{
    uint16_t a = read_register(0);
    uint8_t first = READ_UINT8();
    uint8_t second = READ_UINT8();

    printf("%-15s R%d, #%u, #%u\n", name, a, first, second);
}

static void print_load_call(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t operands = READ_UINT16();

    printf("%-15s R%d, #%u, #%u\n", name, a, LOAD_CALL_OPERAND(operands), LOAD_CALL_FUNCTION(operands));
}

static void print_signed_load_call(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t operands = READ_UINT16();

    printf("%-15s R%d, #%d, #%u\n", name, a, LOAD_CALL_SIGNED_OPERAND(operands), LOAD_CALL_FUNCTION(operands));
}

static void print_opcode_branch(const char* name)
{
    uint16_t a = read_register(0);
    uint16_t b = read_register(1);
    int8_t offset = READ_INT8();

    printf("%-15s R%d, R%d, #%d\n", name, a, b, offset);
}

static void print_opcode_int24(const char* name)
{
    int32_t imm = READ_INT24();

    printf("%-15s #%d\n", name, imm);
}

static void print_instruction(uint8_t c)
{
    switch (c) {
        case OpHlt:            print_opcode("HLT"); break;
        case OpNop:            print_opcode("NOP"); break;
        case OpMov:            print_opcode_registers2("MOV"); break;
        case OpLdi:            print_opcode_register_int16("LDI"); break;
        case OpLdc:            print_opcode_register_uint16("LDC"); break;
        case OpLdg:            print_opcode_register_uint16("LDG"); break;
        case OpStg:            print_opcode_register_uint16("STG"); break;
        case OpStgi:           print_opcode_register_int8_uint8("STGI"); break;
        case OpRefl:           print_opcode_registers2("REFL"); break;
        case OpRefg:           print_opcode_register_uint16("REFG"); break;
        case OpLdr:            print_opcode_registers2("LDR"); break;
        case OpStr:            print_opcode_registers2("STR"); break;
        case OpMov64:         print_opcode_registers2("MOV_64"); break;
        case OpLdc64:         print_opcode_register_uint16("LDC_64"); break;
        case OpLdg64:         print_opcode_register_uint16("LDG_64"); break;
        case OpStg64:         print_opcode_register_uint16("STG_64"); break;
        case OpLdr64:         print_opcode_registers2("LDR_64"); break;
        case OpStr64:         print_opcode_registers2("STR_64"); break;
        case OpSextI32:       print_opcode_registers2_int8("SEXT_I32"); break;
        case OpZextI32:       print_opcode_registers2_int8("ZEXT_I32"); break;
        case OpF32ToF64:     print_opcode_registers2("F32_TO_F64"); break;
        case OpF64ToF32:     print_opcode_registers2("F64_TO_F32"); break;
        case OpF32ToI32:     print_opcode_registers2("F32_TO_I32"); break;
        case OpF32ToU32:     print_opcode_registers2("F32_TO_U32"); break;
        case OpF32ToI64:     print_opcode_registers2("F32_TO_I64"); break;
        case OpF32ToU64:     print_opcode_registers2("F32_TO_U64"); break;
        case OpF64ToI32:     print_opcode_registers2("F64_TO_I32"); break;
        case OpF64ToU32:     print_opcode_registers2("F64_TO_U32"); break;
        case OpF64ToI64:     print_opcode_registers2("F64_TO_I64"); break;
        case OpF64ToU64:     print_opcode_registers2("F64_TO_U64"); break;
        case OpI64ToF32:     print_opcode_registers2("I64_TO_F32"); break;
        case OpI64ToF64:     print_opcode_registers2("I64_TO_F64"); break;
        case OpU64ToF32:     print_opcode_registers2("U64_TO_F32"); break;
        case OpU64ToF64:     print_opcode_registers2("U64_TO_F64"); break;
        case OpI32ToF32:     print_opcode_registers2("INT_TO_F32"); break;
        case OpI32ToF64:     print_opcode_registers2("INT_TO_F64"); break;
        case OpU32ToF32:     print_opcode_registers2("UINT_TO_F32"); break;
        case OpU32ToF64:     print_opcode_registers2("UINT_TO_F64"); break;
        case OpAddF32:        print_opcode_registers3("ADD_F32"); break;
        case OpAddF64:        print_opcode_registers3("ADD_F64"); break;
        case OpSubF32:        print_opcode_registers3("SUB_F32"); break;
        case OpSubF64:        print_opcode_registers3("SUB_F64"); break;
        case OpMulF32:        print_opcode_registers3("MUL_F32"); break;
        case OpMulF64:        print_opcode_registers3("MUL_F64"); break;
        case OpDivF32:        print_opcode_registers3("DIV_F32"); break;
        case OpDivF64:        print_opcode_registers3("DIV_F64"); break;
        case OpRemF32:        print_opcode_registers3("REM_F32"); break;
        case OpRemF64:        print_opcode_registers3("REM_F64"); break;
        case OpNegF32:        print_opcode_registers2("NEG_F32"); break;
        case OpNegF64:        print_opcode_registers2("NEG_F64"); break;
        case OpAddI64:        print_opcode_registers3("ADD_I64"); break;
        case OpAddI32:        print_opcode_registers3("ADD_I32"); break;
        case OpAddI16:        print_opcode_registers3("ADD_I16"); break;
        case OpAddI8:         print_opcode_registers3("ADD_I8"); break;
        case OpAddiI64:       print_opcode_registers2_int8("ADDI_I64"); break;
        case OpAddiI32:       print_opcode_registers2_int8("ADDI_I32"); break;
        case OpAddiI16:       print_opcode_registers2_int8("ADDI_I16"); break;
        case OpAddiI8:        print_opcode_registers2_int8("ADDI_I8"); break;
        case OpSubI64:        print_opcode_registers3("SUB_I64"); break;
        case OpSubI32:        print_opcode_registers3("SUB_I32"); break;
        case OpSubI16:        print_opcode_registers3("SUB_I16"); break;
        case OpSubI8:         print_opcode_registers3("SUB_I8"); break;
        case OpSubiI64:       print_opcode_registers2_int8("SUBI_I64"); break;
        case OpSubiI32:       print_opcode_registers2_int8("SUBI_I32"); break;
        case OpSubiI16:       print_opcode_registers2_int8("SUBI_I16"); break;
        case OpSubiI8:        print_opcode_registers2_int8("SUBI_I8"); break;
        case OpMulI64:        print_opcode_registers3("MUL_I64"); break;
        case OpMuliI64:       print_opcode_registers2_int8("MULI_I64"); break;
        case OpMulI32:        print_opcode_registers3("MUL_I32"); break;
        case OpMuliI32:       print_opcode_registers2_int8("MULI_I32"); break;
        case OpMulI16:        print_opcode_registers3("MUL_I16"); break;
        case OpMuliI16:       print_opcode_registers2_int8("MULI_I16"); break;
        case OpMulI8:         print_opcode_registers3("MUL_I8"); break;
        case OpMuliI8:        print_opcode_registers2_int8("MULI_I8"); break;
        case OpIdivI64:       print_opcode_registers3("IDIV_I64"); break;
        case OpIdivU64:       print_opcode_registers3("IDIV_U64"); break;
        case OpIdivI32:       print_opcode_registers3("IDIV_I32"); break;
        case OpIdivU32:       print_opcode_registers3("IDIV_U32"); break;
        case OpIdivI16:       print_opcode_registers3("IDIV_I16"); break;
        case OpIdivU16:       print_opcode_registers3("IDIV_U16"); break;
        case OpIdivI8:        print_opcode_registers3("IDIV_I8"); break;
        case OpIdivU8:        print_opcode_registers3("IDIV_U8"); break;
        case OpRemI64:        print_opcode_registers3("REM_I64"); break;
        case OpRemU64:        print_opcode_registers3("REM_U64"); break;
        case OpRemI32:        print_opcode_registers3("REM_I32"); break;
        case OpRemU32:        print_opcode_registers3("REM_U32"); break;
        case OpRemI16:        print_opcode_registers3("REM_I16"); break;
        case OpRemU16:        print_opcode_registers3("REM_U16"); break;
        case OpRemI8:         print_opcode_registers3("REM_I8"); break;
        case OpRemU8:         print_opcode_registers3("REM_U8"); break;
        case OpBandI64:       print_opcode_registers3("BAND_I64"); break;
        case OpBandI32:       print_opcode_registers3("BAND_I32"); break;
        case OpBandI16:       print_opcode_registers3("BAND_I16"); break;
        case OpBandI8:        print_opcode_registers3("BAND_I8"); break;
        case OpBandiI64:      print_opcode_registers2_int8("BANDI_I64"); break;
        case OpBandiI32:      print_opcode_registers2_int8("BANDI_I32"); break;
        case OpBandiI16:      print_opcode_registers2_int8("BANDI_I16"); break;
        case OpBandiI8:       print_opcode_registers2_int8("BANDI_I8"); break;
        case OpBorI64:        print_opcode_registers3("BOR_I64"); break;
        case OpBorI32:        print_opcode_registers3("BOR_I32"); break;
        case OpBorI16:        print_opcode_registers3("BOR_I16"); break;
        case OpBorI8:         print_opcode_registers3("BOR_I8"); break;
        case OpBoriI64:       print_opcode_registers2_int8("BORI_I64"); break;
        case OpBoriI32:       print_opcode_registers2_int8("BORI_I32"); break;
        case OpBoriI16:       print_opcode_registers2_int8("BORI_I16"); break;
        case OpBoriI8:        print_opcode_registers2_int8("BORI_I8"); break;
        case OpBxorI64:       print_opcode_registers3("BXOR_I64"); break;
        case OpBxorI32:       print_opcode_registers3("BXOR_I32"); break;
        case OpBxorI16:       print_opcode_registers3("BXOR_I16"); break;
        case OpBxorI8:        print_opcode_registers3("BXOR_I8"); break;
        case OpBxoriI64:      print_opcode_registers2_int8("BXORI_I64"); break;
        case OpBxoriI32:      print_opcode_registers2_int8("BXORI_I32"); break;
        case OpBxoriI16:      print_opcode_registers2_int8("BXORI_I16"); break;
        case OpBxoriI8:       print_opcode_registers2_int8("BXORI_I8"); break;
        case OpBnotI64:       print_opcode_registers2("BNOT_I64"); break;
        case OpBnotI32:       print_opcode_registers2("BNOT_I32"); break;
        case OpBnotI16:       print_opcode_registers2("BNOT_I16"); break;
        case OpBnotI8:        print_opcode_registers2("BNOT_I8"); break;
        case OpLslI64:        print_opcode_registers3("LSL_I64"); break;
        case OpLslI32:        print_opcode_registers3("LSL_I32"); break;
        case OpLslI16:        print_opcode_registers3("LSL_I16"); break;
        case OpLslI8:         print_opcode_registers3("LSL_I8"); break;
        case OpLsliI64:       print_opcode_registers2_int8("LSLI_I64"); break;
        case OpLsliI32:       print_opcode_registers2_int8("LSLI_I32"); break;
        case OpLsliI16:       print_opcode_registers2_int8("LSLI_I16"); break;
        case OpLsliI8:        print_opcode_registers2_int8("LSLI_I8"); break;
        case OpLsrI64:        print_opcode_registers3("LSR_I64"); break;
        case OpLsrI32:        print_opcode_registers3("LSR_I32"); break;
        case OpLsrI16:        print_opcode_registers3("LSR_I16"); break;
        case OpLsrI8:         print_opcode_registers3("LSR_I8"); break;
        case OpLsriI64:       print_opcode_registers2_int8("LSRI_I64"); break;
        case OpLsriI32:       print_opcode_registers2_int8("LSRI_I32"); break;
        case OpLsriI16:       print_opcode_registers2_int8("LSRI_I16"); break;
        case OpLsriI8:        print_opcode_registers2_int8("LSRI_I8"); break;
        case OpAsrI64:        print_opcode_registers3("ASR_I64"); break;
        case OpAsrI32:        print_opcode_registers3("ASR_I32"); break;
        case OpAsrI16:        print_opcode_registers3("ASR_I16"); break;
        case OpAsrI8:         print_opcode_registers3("ASR_I8"); break;
        case OpAsriI64:       print_opcode_registers2_int8("ASRI_I64"); break;
        case OpAsriI32:       print_opcode_registers2_int8("ASRI_I32"); break;
        case OpAsriI16:       print_opcode_registers2_int8("ASRI_I16"); break;
        case OpAsriI8:        print_opcode_registers2_int8("ASRI_I8"); break;
        case OpAnd:            print_opcode_registers3("AND"); break;
        case OpOr:             print_opcode_registers3("OR"); break;
        case OpNot:            print_opcode_registers2("NOT"); break;
        case OpNegI64:        print_opcode_registers2("NEG_I64"); break;
        case OpNegI32:        print_opcode_registers2("NEG_I32"); break;
        case OpNegI16:        print_opcode_registers2("NEG_I16"); break;
        case OpNegI8:         print_opcode_registers2("NEG_I8"); break;
        case OpEq:             print_opcode_registers3("EQ"); break;
        case OpEqi:            print_opcode_registers2_int8("EQI"); break;
        case OpNe:             print_opcode_registers3("NE"); break;
        case OpNei:            print_opcode_registers2_int8("NEI"); break;
        case OpCmpI32:        print_opcode_registers3("CMP_I32"); break;
        case OpCmpU32:        print_opcode_registers3("CMP_U32"); break;
        case OpCmpI64:        print_opcode_registers3("CMP_I64"); break;
        case OpCmpU64:        print_opcode_registers3("CMP_U64"); break;
        case OpCmplF32:       print_opcode_registers3("CMPL_F32"); break;
        case OpCmpgF32:       print_opcode_registers3("CMPG_F32"); break;
        case OpCmplF64:       print_opcode_registers3("CMPL_F64"); break;
        case OpCmpgF64:       print_opcode_registers3("CMPG_F64"); break;
        case OpLtI32:         print_opcode_registers3("LT_I32"); break;
        case OpLtU32:         print_opcode_registers3("LT_U32"); break;
        case OpLeI32:         print_opcode_registers3("LE_I32"); break;
        case OpLeU32:         print_opcode_registers3("LE_U32"); break;
        case OpBz:             print_opcode_register_int16("BZ"); break;
        case OpBnz:            print_opcode_register_int16("BNZ"); break;
        case OpBeq:            print_opcode_branch("BEQ"); break;
        case OpBeqi:           print_opcode_register_int8_int8("BEQI"); break;
        case OpBne:            print_opcode_branch("BNE"); break;
        case OpBnei:           print_opcode_register_int8_int8("BNEI"); break;
        case OpBltI32:        print_opcode_branch("BLT_I32"); break;
        case OpBltU32:        print_opcode_branch("BLT_U32"); break;
        case OpBleI32:        print_opcode_branch("BLE_I32"); break;
        case OpBleU32:        print_opcode_branch("BLE_U32"); break;
        case OpJmp:            print_opcode_int24("JMP"); break;
        case OpCall:           print_opcode_register_uint16("CALL"); break;
        case OpRet:            print_opcode("RET"); break;
        case OpRet64:         print_opcode_register("RET_64"); break;
        case OpRetv:           print_opcode_register("RETV"); break;
        case OpCallCall:      print_opcode_register_uint8_uint8("CALL_CALL"); break;
        case OpLdiLdi:        print_opcode_register_int8_int8("LDI_LDI"); break;
        case OpLdcLdc:        print_opcode_register_uint8_uint8("LDC_LDC"); break;
        case OpLdgLdg:        print_opcode_register_uint8_uint8("LDG_LDG"); break;
        case OpMovMov:        print_opcode_registers3("MOV_MOV"); break;
        case OpLdiCall:       print_signed_load_call("LDI_CALL"); break;
        case OpLdcCall:       print_load_call("LDC_CALL"); break;
        case OpLdgCall:       print_load_call("LDG_CALL"); break;
        case OpLdc64Call:    print_load_call("LDC_64_CALL"); break;
        case OpLdg64Call:    print_load_call("LDG_64_CALL"); break;
        case OpLdrCall:       print_load_call("LDR_CALL"); break;
        case OpStgCall:       print_opcode_register_uint8_uint8("STG_CALL"); break;
        case OpLdiLdg:        print_opcode_register_int8_uint8("LDI_LDG"); break;
        case OpLdgLdi:        print_opcode_register_uint8_int8("LDG_LDI"); break;
        case OpMovLdi:        print_opcode_registers2_int8("MOV_LDI"); break;
        case OpMovLdc:        print_opcode_registers2_uint8("MOV_LDC"); break;
        case OpMovLdg:        print_opcode_registers2_uint8("MOV_LDG"); break;
        default:
            unknown_opcode_error(c);
    }
}

static void disassemble_code(const CodeObject* code)
{
    ptr = code->data;

    while (ptr != code_object_end(code)) {
        register_high[0] = 0;
        register_high[1] = 0;
        register_high[2] = 0;
        uint8_t c = READ_UINT8();

        if (c == OpWide) {
            print_wide_prefix();
            c = READ_UINT8();
        }

        print_instruction(c);
    }
}

static void disassemble_function(const FunctionObject* function, bool separate)
{
    if (function->type == FuncBuiltin) {
        return;
    }

    disassemble_code(&function->code);

    if (separate) {
        printf("\n");
    }
}

void disassemble_module(const ModuleObject* module)
{
    size_t function_count = count_vector(&module->functions);

    for (size_t i = 0; i < function_count; i++) {
        const FunctionObject* function = get_vector_at(&module->functions, i);
        bool separate = i < function_count - 1;

        disassemble_function(function, separate);
    }
}
