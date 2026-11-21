#include "vm/disassembler.h"
#include "common/errors.h"
#include "runtime/code_object.h"
#include "runtime/function_object.h"
#include "vm/opcode.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct DecodedInstruction
{
    Instruction inst;
    uint16_t a;
    uint16_t b;
    uint16_t c;
} DecodedInstruction;

static _Noreturn void unknown_opcode_error(int opcode)
{
    fatal_error("Unknown opcode %d", opcode);
}

static void print_wide_prefix(Instruction prefix)
{
    uint8_t a = OPERAND_A(prefix);
    uint8_t b = OPERAND_B(prefix);
    uint8_t c = OPERAND_C(prefix);

    printf("%-15s #%u, #%u, #%u\n", "WIDE", a, b, c);
}

static void print_opcode(const char* name)
{
    printf("%s\n", name);
}

static void print_opcode_a(const char* name, const DecodedInstruction* operands)
{
    printf("%-15s R%d\n", name, operands->a);
}

static void print_opcode_ab(const char* name, const DecodedInstruction* operands)
{
    printf("%-15s R%d, R%d\n", name, operands->a, operands->b);
}

static void print_opcode_abc(const char* name, const DecodedInstruction* operands)
{
    printf("%-15s R%d, R%d, R%d\n", name, operands->a, operands->b, operands->c);
}

static void print_opcode_ab_i8(const char* name, const DecodedInstruction* operands)
{
    int8_t imm = OPERAND_C(operands->inst);

    printf("%-15s R%d, R%d, #%d\n", name, operands->a, operands->b, imm);
}

static void print_opcode_ab_u8(const char* name, const DecodedInstruction* operands)
{
    uint8_t index = OPERAND_C(operands->inst);

    printf("%-15s R%d, R%d, #%u\n", name, operands->a, operands->b, index);
}

static void print_opcode_a_i16(const char* name, const DecodedInstruction* operands)
{
    int16_t imm = OPERAND_BC(operands->inst);

    printf("%-15s R%d, #%d\n", name, operands->a, imm);
}

static void print_opcode_a_u16(const char* name, const DecodedInstruction* operands)
{
    uint16_t imm = OPERAND_BC(operands->inst);

    printf("%-15s R%d, #%u\n", name, operands->a, imm);
}

static void print_opcode_a_i8_u8(const char* name, const DecodedInstruction* operands)
{
    int8_t imm = OPERAND_B(operands->inst);
    uint8_t position = OPERAND_C(operands->inst);

    printf("%-15s R%d, #%d, #%u\n", name, operands->a, imm, position);
}

static void print_opcode_a_u8_i8(const char* name, const DecodedInstruction* operands)
{
    uint8_t position = OPERAND_B(operands->inst);
    int8_t imm = OPERAND_C(operands->inst);

    printf("%-15s R%d, #%u, #%d\n", name, operands->a, position, imm);
}

static void print_opcode_a_i8_i8(const char* name, const DecodedInstruction* operands)
{
    int8_t first = OPERAND_B(operands->inst);
    int8_t second = OPERAND_C(operands->inst);

    printf("%-15s R%d, #%d, #%d\n", name, operands->a, first, second);
}

static void print_opcode_a_u8_u8(const char* name, const DecodedInstruction* operands)
{
    uint8_t first = OPERAND_B(operands->inst);
    uint8_t second = OPERAND_C(operands->inst);

    printf("%-15s R%d, #%u, #%u\n", name, operands->a, first, second);
}

static void print_load_call(const char* name, const DecodedInstruction* operands)
{
    uint16_t packed = OPERAND_BC(operands->inst);
    uint16_t value = LOAD_CALL_OPERAND(packed);
    uint8_t function = LOAD_CALL_FUNCTION(packed);

    printf("%-15s R%d, #%u, #%u\n", name, operands->a, value, function);
}

static void print_signed_load_call(const char* name, const DecodedInstruction* operands)
{
    uint16_t packed = OPERAND_BC(operands->inst);
    int16_t value = LOAD_CALL_SIGNED_OPERAND(packed);
    uint8_t function = LOAD_CALL_FUNCTION(packed);

    printf("%-15s R%d, #%d, #%u\n", name, operands->a, value, function);
}

static void print_opcode_i24(const char* name, const DecodedInstruction* operands)
{
    int32_t imm = SIGNED_OPERAND_ABC(operands->inst);

    printf("%-15s #%d\n", name, imm);
}

static void print_instruction(Instruction inst, Instruction prefix)
{
    DecodedInstruction operands = {
        .inst = inst,
        .a = ((uint16_t)OPERAND_A(prefix) << 8) | OPERAND_A(inst),
        .b = ((uint16_t)OPERAND_B(prefix) << 8) | OPERAND_B(inst),
        .c = ((uint16_t)OPERAND_C(prefix) << 8) | OPERAND_C(inst)
    };

    Opcode opcode = (Opcode)OPCODE(inst);

    switch (opcode) {
        case OpHlt:         print_opcode("HLT"); break;
        case OpNop:         print_opcode("NOP"); break;
        case OpMov:         print_opcode_ab("MOV", &operands); break;
        case OpLdi:         print_opcode_a_i16("LDI", &operands); break;
        case OpLdc:         print_opcode_a_u16("LDC", &operands); break;
        case OpLdg:         print_opcode_a_u16("LDG", &operands); break;
        case OpStg:         print_opcode_a_u16("STG", &operands); break;
        case OpStgi:        print_opcode_a_i8_u8("STGI", &operands); break;
        case OpRefl:        print_opcode_ab("REFL", &operands); break;
        case OpRefg:        print_opcode_a_u16("REFG", &operands); break;
        case OpLdr:         print_opcode_ab("LDR", &operands); break;
        case OpStr:         print_opcode_ab("STR", &operands); break;
        case OpMov64:       print_opcode_ab("MOV_64", &operands); break;
        case OpLdc64:       print_opcode_a_u16("LDC_64", &operands); break;
        case OpLdg64:       print_opcode_a_u16("LDG_64", &operands); break;
        case OpStg64:       print_opcode_a_u16("STG_64", &operands); break;
        case OpLdr64:       print_opcode_ab("LDR_64", &operands); break;
        case OpStr64:       print_opcode_ab("STR_64", &operands); break;
        case OpLdm8:        print_opcode_abc("LDM_8", &operands); break;
        case OpLdmi8:       print_opcode_ab_u8("LDMI_8", &operands); break;
        case OpLdm16:       print_opcode_abc("LDM_16", &operands); break;
        case OpLdmi16:      print_opcode_ab_u8("LDMI_16", &operands); break;
        case OpLdm32:       print_opcode_abc("LDM_32", &operands); break;
        case OpLdmi32:      print_opcode_ab_u8("LDMI_32", &operands); break;
        case OpLdm64:       print_opcode_abc("LDM_64", &operands); break;
        case OpLdmi64:      print_opcode_ab_u8("LDMI_64", &operands); break;
        case OpStm8:        print_opcode_abc("STM_8", &operands); break;
        case OpStmi8:       print_opcode_ab_u8("STMI_8", &operands); break;
        case OpStm16:       print_opcode_abc("STM_16", &operands); break;
        case OpStmi16:      print_opcode_ab_u8("STMI_16", &operands); break;
        case OpStm32:       print_opcode_abc("STM_32", &operands); break;
        case OpStmi32:      print_opcode_ab_u8("STMI_32", &operands); break;
        case OpStm64:       print_opcode_abc("STM_64", &operands); break;
        case OpStmi64:      print_opcode_ab_u8("STMI_64", &operands); break;
        case OpSextI32:     print_opcode_ab_i8("SEXT_I32", &operands); break;
        case OpZextI32:     print_opcode_ab_i8("ZEXT_I32", &operands); break;
        case OpF32ToF64:    print_opcode_ab("F32_TO_F64", &operands); break;
        case OpF64ToF32:    print_opcode_ab("F64_TO_F32", &operands); break;
        case OpF32ToI32:    print_opcode_ab("F32_TO_I32", &operands); break;
        case OpF32ToU32:    print_opcode_ab("F32_TO_U32", &operands); break;
        case OpF32ToI64:    print_opcode_ab("F32_TO_I64", &operands); break;
        case OpF32ToU64:    print_opcode_ab("F32_TO_U64", &operands); break;
        case OpF64ToI32:    print_opcode_ab("F64_TO_I32", &operands); break;
        case OpF64ToU32:    print_opcode_ab("F64_TO_U32", &operands); break;
        case OpF64ToI64:    print_opcode_ab("F64_TO_I64", &operands); break;
        case OpF64ToU64:    print_opcode_ab("F64_TO_U64", &operands); break;
        case OpI64ToF32:    print_opcode_ab("I64_TO_F32", &operands); break;
        case OpI64ToF64:    print_opcode_ab("I64_TO_F64", &operands); break;
        case OpU64ToF32:    print_opcode_ab("U64_TO_F32", &operands); break;
        case OpU64ToF64:    print_opcode_ab("U64_TO_F64", &operands); break;
        case OpI32ToF32:    print_opcode_ab("INT_TO_F32", &operands); break;
        case OpI32ToF64:    print_opcode_ab("INT_TO_F64", &operands); break;
        case OpU32ToF32:    print_opcode_ab("UINT_TO_F32", &operands); break;
        case OpU32ToF64:    print_opcode_ab("UINT_TO_F64", &operands); break;
        case OpAddF32:      print_opcode_abc("ADD_F32", &operands); break;
        case OpAddF64:      print_opcode_abc("ADD_F64", &operands); break;
        case OpSubF32:      print_opcode_abc("SUB_F32", &operands); break;
        case OpSubF64:      print_opcode_abc("SUB_F64", &operands); break;
        case OpMulF32:      print_opcode_abc("MUL_F32", &operands); break;
        case OpMulF64:      print_opcode_abc("MUL_F64", &operands); break;
        case OpDivF32:      print_opcode_abc("DIV_F32", &operands); break;
        case OpDivF64:      print_opcode_abc("DIV_F64", &operands); break;
        case OpRemF32:      print_opcode_abc("REM_F32", &operands); break;
        case OpRemF64:      print_opcode_abc("REM_F64", &operands); break;
        case OpNegF32:      print_opcode_ab("NEG_F32", &operands); break;
        case OpNegF64:      print_opcode_ab("NEG_F64", &operands); break;
        case OpAddI64:      print_opcode_abc("ADD_I64", &operands); break;
        case OpAddiI64:     print_opcode_ab_i8("ADDI_I64", &operands); break;
        case OpAddI32:      print_opcode_abc("ADD_I32", &operands); break;
        case OpAddiI32:     print_opcode_ab_i8("ADDI_I32", &operands); break;
        case OpAddI16:      print_opcode_abc("ADD_I16", &operands); break;
        case OpAddiI16:     print_opcode_ab_i8("ADDI_I16", &operands); break;
        case OpAddI8:       print_opcode_abc("ADD_I8", &operands); break;
        case OpAddiI8:      print_opcode_ab_i8("ADDI_I8", &operands); break;
        case OpSubI64:      print_opcode_abc("SUB_I64", &operands); break;
        case OpSubiI64:     print_opcode_ab_i8("SUBI_I64", &operands); break;
        case OpSubI32:      print_opcode_abc("SUB_I32", &operands); break;
        case OpSubiI32:     print_opcode_ab_i8("SUBI_I32", &operands); break;
        case OpSubI16:      print_opcode_abc("SUB_I16", &operands); break;
        case OpSubiI16:     print_opcode_ab_i8("SUBI_I16", &operands); break;
        case OpSubI8:       print_opcode_abc("SUB_I8", &operands); break;
        case OpSubiI8:      print_opcode_ab_i8("SUBI_I8", &operands); break;
        case OpMulI64:      print_opcode_abc("MUL_I64", &operands); break;
        case OpMuliI64:     print_opcode_ab_i8("MULI_I64", &operands); break;
        case OpMulI32:      print_opcode_abc("MUL_I32", &operands); break;
        case OpMuliI32:     print_opcode_ab_i8("MULI_I32", &operands); break;
        case OpMulI16:      print_opcode_abc("MUL_I16", &operands); break;
        case OpMuliI16:     print_opcode_ab_i8("MULI_I16", &operands); break;
        case OpMulI8:       print_opcode_abc("MUL_I8", &operands); break;
        case OpMuliI8:      print_opcode_ab_i8("MULI_I8", &operands); break;
        case OpIdivI64:     print_opcode_abc("IDIV_I64", &operands); break;
        case OpIdivU64:     print_opcode_abc("IDIV_U64", &operands); break;
        case OpIdivI32:     print_opcode_abc("IDIV_I32", &operands); break;
        case OpIdivU32:     print_opcode_abc("IDIV_U32", &operands); break;
        case OpIdivI16:     print_opcode_abc("IDIV_I16", &operands); break;
        case OpIdivU16:     print_opcode_abc("IDIV_U16", &operands); break;
        case OpIdivI8:      print_opcode_abc("IDIV_I8", &operands); break;
        case OpIdivU8:      print_opcode_abc("IDIV_U8", &operands); break;
        case OpRemI64:      print_opcode_abc("REM_I64", &operands); break;
        case OpRemU64:      print_opcode_abc("REM_U64", &operands); break;
        case OpRemI32:      print_opcode_abc("REM_I32", &operands); break;
        case OpRemU32:      print_opcode_abc("REM_U32", &operands); break;
        case OpRemI16:      print_opcode_abc("REM_I16", &operands); break;
        case OpRemU16:      print_opcode_abc("REM_U16", &operands); break;
        case OpRemI8:       print_opcode_abc("REM_I8", &operands); break;
        case OpRemU8:       print_opcode_abc("REM_U8", &operands); break;
        case OpBandI64:     print_opcode_abc("BAND_I64", &operands); break;
        case OpBandiI64:    print_opcode_ab_i8("BANDI_I64", &operands); break;
        case OpBandI32:     print_opcode_abc("BAND_I32", &operands); break;
        case OpBandiI32:    print_opcode_ab_i8("BANDI_I32", &operands); break;
        case OpBandI16:     print_opcode_abc("BAND_I16", &operands); break;
        case OpBandiI16:    print_opcode_ab_i8("BANDI_I16", &operands); break;
        case OpBandI8:      print_opcode_abc("BAND_I8", &operands); break;
        case OpBandiI8:     print_opcode_ab_i8("BANDI_I8", &operands); break;
        case OpBorI64:      print_opcode_abc("BOR_I64", &operands); break;
        case OpBoriI64:     print_opcode_ab_i8("BORI_I64", &operands); break;
        case OpBorI32:      print_opcode_abc("BOR_I32", &operands); break;
        case OpBoriI32:     print_opcode_ab_i8("BORI_I32", &operands); break;
        case OpBorI16:      print_opcode_abc("BOR_I16", &operands); break;
        case OpBoriI16:     print_opcode_ab_i8("BORI_I16", &operands); break;
        case OpBorI8:       print_opcode_abc("BOR_I8", &operands); break;
        case OpBoriI8:      print_opcode_ab_i8("BORI_I8", &operands); break;
        case OpBxorI64:     print_opcode_abc("BXOR_I64", &operands); break;
        case OpBxoriI64:    print_opcode_ab_i8("BXORI_I64", &operands); break;
        case OpBxorI32:     print_opcode_abc("BXOR_I32", &operands); break;
        case OpBxoriI32:    print_opcode_ab_i8("BXORI_I32", &operands); break;
        case OpBxorI16:     print_opcode_abc("BXOR_I16", &operands); break;
        case OpBxoriI16:    print_opcode_ab_i8("BXORI_I16", &operands); break;
        case OpBxorI8:      print_opcode_abc("BXOR_I8", &operands); break;
        case OpBxoriI8:     print_opcode_ab_i8("BXORI_I8", &operands); break;
        case OpBnotI64:     print_opcode_ab("BNOT_I64", &operands); break;
        case OpBnotI32:     print_opcode_ab("BNOT_I32", &operands); break;
        case OpBnotI16:     print_opcode_ab("BNOT_I16", &operands); break;
        case OpBnotI8:      print_opcode_ab("BNOT_I8", &operands); break;
        case OpLslI64:      print_opcode_abc("LSL_I64", &operands); break;
        case OpLsliI64:     print_opcode_ab_i8("LSLI_I64", &operands); break;
        case OpLslI32:      print_opcode_abc("LSL_I32", &operands); break;
        case OpLsliI32:     print_opcode_ab_i8("LSLI_I32", &operands); break;
        case OpLslI16:      print_opcode_abc("LSL_I16", &operands); break;
        case OpLsliI16:     print_opcode_ab_i8("LSLI_I16", &operands); break;
        case OpLslI8:       print_opcode_abc("LSL_I8", &operands); break;
        case OpLsliI8:      print_opcode_ab_i8("LSLI_I8", &operands); break;
        case OpLsrI64:      print_opcode_abc("LSR_I64", &operands); break;
        case OpLsriI64:     print_opcode_ab_i8("LSRI_I64", &operands); break;
        case OpLsrI32:      print_opcode_abc("LSR_I32", &operands); break;
        case OpLsriI32:     print_opcode_ab_i8("LSRI_I32", &operands); break;
        case OpLsrI16:      print_opcode_abc("LSR_I16", &operands); break;
        case OpLsriI16:     print_opcode_ab_i8("LSRI_I16", &operands); break;
        case OpLsrI8:       print_opcode_abc("LSR_I8", &operands); break;
        case OpLsriI8:      print_opcode_ab_i8("LSRI_I8", &operands); break;
        case OpAsrI64:      print_opcode_abc("ASR_I64", &operands); break;
        case OpAsriI64:     print_opcode_ab_i8("ASRI_I64", &operands); break;
        case OpAsrI32:      print_opcode_abc("ASR_I32", &operands); break;
        case OpAsriI32:     print_opcode_ab_i8("ASRI_I32", &operands); break;
        case OpAsrI16:      print_opcode_abc("ASR_I16", &operands); break;
        case OpAsriI16:     print_opcode_ab_i8("ASRI_I16", &operands); break;
        case OpAsrI8:       print_opcode_abc("ASR_I8", &operands); break;
        case OpAsriI8:      print_opcode_ab_i8("ASRI_I8", &operands); break;
        case OpAnd:         print_opcode_abc("AND", &operands); break;
        case OpOr:          print_opcode_abc("OR", &operands); break;
        case OpNot:         print_opcode_ab("NOT", &operands); break;
        case OpNegI64:      print_opcode_ab("NEG_I64", &operands); break;
        case OpNegI32:      print_opcode_ab("NEG_I32", &operands); break;
        case OpNegI16:      print_opcode_ab("NEG_I16", &operands); break;
        case OpNegI8:       print_opcode_ab("NEG_I8", &operands); break;
        case OpEq:          print_opcode_abc("EQ", &operands); break;
        case OpEqi:         print_opcode_ab_i8("EQI", &operands); break;
        case OpNe:          print_opcode_abc("NE", &operands); break;
        case OpNei:         print_opcode_ab_i8("NEI", &operands); break;
        case OpEqString:    print_opcode_abc("EQ_STRING", &operands); break;
        case OpNeString:    print_opcode_abc("NE_STRING", &operands); break;
        case OpLtI32:       print_opcode_abc("LT_I32", &operands); break;
        case OpLtU32:       print_opcode_abc("LT_U32", &operands); break;
        case OpLeI32:       print_opcode_abc("LE_I32", &operands); break;
        case OpLeU32:       print_opcode_abc("LE_U32", &operands); break;
        case OpCmpI32:      print_opcode_abc("CMP_I32", &operands); break;
        case OpCmpU32:      print_opcode_abc("CMP_U32", &operands); break;
        case OpCmpI64:      print_opcode_abc("CMP_I64", &operands); break;
        case OpCmpU64:      print_opcode_abc("CMP_U64", &operands); break;
        case OpCmplF32:     print_opcode_abc("CMPL_F32", &operands); break;
        case OpCmpgF32:     print_opcode_abc("CMPG_F32", &operands); break;
        case OpCmplF64:     print_opcode_abc("CMPL_F64", &operands); break;
        case OpCmpgF64:     print_opcode_abc("CMPG_F64", &operands); break;
        case OpBz:          print_opcode_a_i16("BZ", &operands); break;
        case OpBnz:         print_opcode_a_i16("BNZ", &operands); break;
        case OpBeq:         print_opcode_ab_i8("BEQ", &operands); break;
        case OpBeqi:        print_opcode_a_i8_i8("BEQI", &operands); break;
        case OpBne:         print_opcode_ab_i8("BNE", &operands); break;
        case OpBnei:        print_opcode_a_i8_i8("BNEI", &operands); break;
        case OpBltI32:      print_opcode_ab_i8("BLT_I32", &operands); break;
        case OpBltU32:      print_opcode_ab_i8("BLT_U32", &operands); break;
        case OpBleI32:      print_opcode_ab_i8("BLE_I32", &operands); break;
        case OpBleU32:      print_opcode_ab_i8("BLE_U32", &operands); break;
        case OpJmp:         print_opcode_i24("JMP", &operands); break;
        case OpCall:        print_opcode_a_u16("CALL", &operands); break;
        case OpRet:         print_opcode("RET"); break;
        case OpRet64:       print_opcode_a("RET_64", &operands); break;
        case OpRetv:        print_opcode_a("RETV", &operands); break;
        case OpCallCall:    print_opcode_a_u8_u8("CALL_CALL", &operands); break;
        case OpLdiLdi:      print_opcode_a_i8_i8("LDI_LDI", &operands); break;
        case OpLdcLdc:      print_opcode_a_u8_u8("LDC_LDC", &operands); break;
        case OpLdgLdg:      print_opcode_a_u8_u8("LDG_LDG", &operands); break;
        case OpMovMov:      print_opcode_abc("MOV_MOV", &operands); break;
        case OpLdiCall:     print_signed_load_call("LDI_CALL", &operands); break;
        case OpLdcCall:     print_load_call("LDC_CALL", &operands); break;
        case OpLdgCall:     print_load_call("LDG_CALL", &operands); break;
        case OpLdc64Call:   print_load_call("LDC_64_CALL", &operands); break;
        case OpLdg64Call:   print_load_call("LDG_64_CALL", &operands); break;
        case OpLdrCall:     print_load_call("LDR_CALL", &operands); break;
        case OpStgCall:     print_opcode_a_u8_u8("STG_CALL", &operands); break;
        case OpLdiLdg:      print_opcode_a_i8_u8("LDI_LDG", &operands); break;
        case OpLdgLdi:      print_opcode_a_u8_i8("LDG_LDI", &operands); break;
        case OpMovLdi:      print_opcode_ab_i8("MOV_LDI", &operands); break;
        case OpMovLdc:      print_opcode_ab_u8("MOV_LDC", &operands); break;
        case OpMovLdg:      print_opcode_ab_u8("MOV_LDG", &operands); break;
        default:
            unknown_opcode_error(opcode);
    }
}

static const Instruction* disassemble_instruction(const Instruction* cursor)
{
    Instruction inst = *cursor++;
    Instruction prefix = 0;

    if (OPCODE(inst) == OpWide) {
        prefix = inst;
        inst = *cursor++;

        print_wide_prefix(prefix);
    }

    print_instruction(inst, prefix);

    return cursor;
}

static void disassemble_code(const CodeObject* code)
{
    const Instruction* cursor = (const Instruction*)code->data;
    const Instruction* end = (const Instruction*)code_object_end(code);

    while (cursor != end) {
        cursor = disassemble_instruction(cursor);
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
