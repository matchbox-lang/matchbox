#include "vm.h"
#include "function_object.h"
#include "opcode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if UINTPTR_MAX == UINT32_MAX
#define VM_WIDE_INSTRUCTIONS "vm/wide_32.inc"
#elif UINTPTR_MAX == UINT64_MAX
#define VM_WIDE_INSTRUCTIONS "vm/wide_64.inc"
#else
#error Unsupported pointer width
#endif

static void stackOverflowError(void)
{
    fprintf(stderr, "Error: Stack overflow\n");
    exit(1);
}

static void divisionByZeroError(void)
{
    fprintf(stderr, "Error: Division by zero\n");
    exit(1);
}

static void testFrameOverflow(VM* vm, Value* frame, int maxStackCount)
{
    if (frame - vm->stack - 2 + maxStackCount > STACK_MAX) {
        stackOverflowError();
    }
}

void enterBytecodeFunction(VM* vm, FunctionObject* function, Value* frame)
{
    testFrameOverflow(vm, frame, function->maxStackCount);

    frame[-2] = POINTER_VALUE(vm->ip);
    frame[-1] = POINTER_VALUE(vm->fp);
    vm->fp = frame;
    vm->ip = (Instruction*)function->code.data;
}

static void run(VM* vm)
{
    uint16_t functionPosition = 0;
    FunctionObject* function = vm->module->functions.data[functionPosition];
    vm->ip = (Instruction*)function->code.data;
    
    if (function->maxStackCount > STACK_MAX) {
        stackOverflowError();
    }

    for (;;) {
        Instruction inst = *vm->ip++;
        Opcode opcode = (Opcode)OPCODE(inst);
        uint8_t a = OPERAND_A(inst);
        uint8_t b = OPERAND_B(inst);
        uint8_t c = OPERAND_C(inst);

        switch (opcode) {
            case OP_HLT:
                return;
            case OP_NOP:
                break;
            case OP_MOV:
                vm->fp[a] = vm->fp[b];
                break;
            case OP_LDI:
                vm->fp[a] = SIGNED_VALUE((int16_t)OPERAND_BC(inst));
                break;
            case OP_LDC:
                vm->fp[a] = vm->module->constants.data[OPERAND_BC(inst)];
                break;
            case OP_LDG:
                vm->fp[a] = vm->gp[OPERAND_BC(inst)];
                break;
            case OP_STG:
                vm->gp[OPERAND_BC(inst)] = vm->fp[a];
                break;
            case OP_REFL:
                vm->fp[a] = POINTER_VALUE(&vm->fp[b]);
                break;
            case OP_REFG:
                vm->fp[a] = POINTER_VALUE(&vm->gp[OPERAND_BC(inst)]);
                break;
            case OP_LDR:
                vm->fp[a] = *(Value*)AS_POINTER(vm->fp[b]);
                break;
            case OP_STR:
                *(Value*)AS_POINTER(vm->fp[b]) = vm->fp[a];
                break;
#include VM_WIDE_INSTRUCTIONS
            case OP_F32_TO_F64: {
                float source = AS_F32(vm->fp[b]);
                writeF64(vm->fp, a, source);
                break;
            }
            case OP_ADD_F32: {
                float result = AS_F32(vm->fp[b]) + AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OP_ADD_F64: {
                double left = readF64(vm->fp, b);
                double right = readF64(vm->fp, c);
                writeF64(vm->fp, a, left + right);
                break;
            }
            case OP_SUB_F32: {
                float result = AS_F32(vm->fp[b]) - AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OP_SUB_F64: {
                double left = readF64(vm->fp, b);
                double right = readF64(vm->fp, c);
                writeF64(vm->fp, a, left - right);
                break;
            }
            case OP_MUL_F32: {
                float result = AS_F32(vm->fp[b]) * AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OP_MUL_F64: {
                double left = readF64(vm->fp, b);
                double right = readF64(vm->fp, c);
                writeF64(vm->fp, a, left * right);
                break;
            }
            case OP_DIV_F32: {
                float result = AS_F32(vm->fp[b]) / AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OP_DIV_F64: {
                double left = readF64(vm->fp, b);
                double right = readF64(vm->fp, c);
                writeF64(vm->fp, a, left / right);
                break;
            }
            case OP_REM_F32: {
                float left = AS_F32(vm->fp[b]);
                float right = AS_F32(vm->fp[c]);
                float result = fmodf(left, right);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OP_REM_F64: {
                double left = readF64(vm->fp, b);
                double right = readF64(vm->fp, c);
                double result = fmod(left, right);
                writeF64(vm->fp, a, result);
                break;
            }
            case OP_NEG_F32: {
                float result = -AS_F32(vm->fp[b]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OP_NEG_F64: {
                double value = readF64(vm->fp, b);
                writeF64(vm->fp, a, -value);
                break;
            }
            case OP_ADD_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) + AS_U32(vm->fp[c]));
                break;
            case OP_ADDI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) + (int8_t)c);
                break;
            case OP_ADD_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) + AS_U16(vm->fp[c]));
                break;
            case OP_ADDI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) + (int8_t)c);
                break;
            case OP_ADD_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) + AS_U8(vm->fp[c]));
                break;
            case OP_ADDI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) + (int8_t)c);
                break;
            case OP_SUB_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) - AS_U32(vm->fp[c]));
                break;
            case OP_SUBI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) - (int8_t)c);
                break;
            case OP_SUB_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) - AS_U16(vm->fp[c]));
                break;
            case OP_SUBI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) - (int8_t)c);
                break;
            case OP_SUB_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) - AS_U8(vm->fp[c]));
                break;
            case OP_SUBI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) - (int8_t)c);
                break;
            case OP_MUL_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) * AS_U32(vm->fp[c]));
                break;
            case OP_MULI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) * (int8_t)c);
                break;
            case OP_MUL_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) * AS_U16(vm->fp[c]));
                break;
            case OP_MULI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) * (int8_t)c);
                break;
            case OP_MUL_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) * AS_U8(vm->fp[c]));
                break;
            case OP_MULI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) * (int8_t)c);
                break;
            case OP_IDIV_I32: {
                if (!AS_I32(vm->fp[c])) {
                    divisionByZeroError();
                }

                if (AS_I32(vm->fp[b]) == INT32_MIN && AS_I32(vm->fp[c]) == -1) {
                    vm->fp[a] = I32_VALUE(INT32_MIN);
                    break;
                }

                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) / AS_I32(vm->fp[c]));
                break;
            }
            case OP_IDIV_I16: {
                if (!AS_I16(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) / AS_I16(vm->fp[c]));
                break;
            }
            case OP_IDIV_I8: {
                if (!AS_I8(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) / AS_I8(vm->fp[c]));
                break;
            }
            case OP_IDIV_U32: {
                if (!AS_U32(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) / AS_U32(vm->fp[c]));
                break;
            }
            case OP_IDIV_U16: {
                if (!AS_U16(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) / AS_U16(vm->fp[c]));
                break;
            }
            case OP_IDIV_U8: {
                if (!AS_U8(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) / AS_U8(vm->fp[c]));
                break;
            }
            case OP_REM_I32: {
                if (!AS_I32(vm->fp[c])) {
                    divisionByZeroError();
                }

                if (AS_I32(vm->fp[b]) == INT32_MIN && AS_I32(vm->fp[c]) == -1) {
                    vm->fp[a] = I32_VALUE(0);
                    break;
                }

                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) % AS_I32(vm->fp[c]));
                break;
            }
            case OP_REM_I16: {
                if (!AS_I16(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) % AS_I16(vm->fp[c]));
                break;
            }
            case OP_REM_I8: {
                if (!AS_I8(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) % AS_I8(vm->fp[c]));
                break;
            }
            case OP_REM_U32: {
                if (!AS_U32(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) % AS_U32(vm->fp[c]));
                break;
            }
            case OP_REM_U16: {
                if (!AS_U16(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) % AS_U16(vm->fp[c]));
                break;
            }
            case OP_REM_U8: {
                if (!AS_U8(vm->fp[c])) {
                    divisionByZeroError();
                }

                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) % AS_U8(vm->fp[c]));
                break;
            }
            case OP_BAND_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) & AS_U32(vm->fp[c]));
                break;
            case OP_BANDI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) & c);
                break;
            case OP_BAND_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) & AS_U16(vm->fp[c]));
                break;
            case OP_BANDI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) & c);
                break;
            case OP_BAND_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) & AS_U8(vm->fp[c]));
                break;
            case OP_BANDI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) & c);
                break;
            case OP_BOR_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) | AS_U32(vm->fp[c]));
                break;
            case OP_BORI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) | c);
                break;
            case OP_BOR_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) | AS_U16(vm->fp[c]));
                break;
            case OP_BORI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) | c);
                break;
            case OP_BOR_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) | AS_U8(vm->fp[c]));
                break;
            case OP_BORI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) | c);
                break;
            case OP_BXOR_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) ^ AS_U32(vm->fp[c]));
                break;
            case OP_BXORI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) ^ c);
                break;
            case OP_BXOR_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) ^ AS_U16(vm->fp[c]));
                break;
            case OP_BXORI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) ^ c);
                break;
            case OP_BXOR_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) ^ AS_U8(vm->fp[c]));
                break;
            case OP_BXORI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) ^ c);
                break;
            case OP_BNOT_I32:
                vm->fp[a] = I32_VALUE(~AS_U32(vm->fp[b]));
                break;
            case OP_BNOT_I16:
                vm->fp[a] = I16_VALUE(~AS_U16(vm->fp[b]));
                break;
            case OP_BNOT_I8:
                vm->fp[a] = I8_VALUE(~AS_U8(vm->fp[b]));
                break;
            case OP_LSL_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) << AS_U32(vm->fp[c]));
                break;
            case OP_LSLI_I32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) << c);
                break;
            case OP_LSL_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) << AS_U16(vm->fp[c]));
                break;
            case OP_LSLI_I16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) << c);
                break;
            case OP_LSL_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) << AS_U8(vm->fp[c]));
                break;
            case OP_LSLI_I8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) << c);
                break;
            case OP_LSR_I32:
                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) >> AS_U32(vm->fp[c]));
                break;
            case OP_LSRI_I32:
                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) >> c);
                break;
            case OP_LSR_I16:
                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) >> AS_U16(vm->fp[c]));
                break;
            case OP_LSRI_I16:
                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) >> c);
                break;
            case OP_LSR_I8:
                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) >> AS_U8(vm->fp[c]));
                break;
            case OP_LSRI_I8:
                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) >> c);
                break;
            case OP_ASR_I32:
                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) >> AS_I32(vm->fp[c]));
                break;
            case OP_ASRI_I32:
                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) >> c);
                break;
            case OP_ASR_I16:
                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) >> AS_I16(vm->fp[c]));
                break;
            case OP_ASRI_I16:
                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) >> c);
                break;
            case OP_ASR_I8:
                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) >> AS_I8(vm->fp[c]));
                break;
            case OP_ASRI_I8:
                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) >> c);
                break;
            case OP_AND:
                vm->fp[a] = BOOL_VALUE(AS_BOOL(vm->fp[b]) && AS_BOOL(vm->fp[c]));
                break;
            case OP_OR:
                vm->fp[a] = BOOL_VALUE(AS_BOOL(vm->fp[b]) || AS_BOOL(vm->fp[c]));
                break;
            case OP_NOT:
                vm->fp[a] = SIGNED_VALUE(!AS_SIGNED(vm->fp[b]));
                break;
            case OP_NEG_I32:
                vm->fp[a] = SIGNED_VALUE(-AS_SIGNED(vm->fp[b]));
                break;
            case OP_NEG_I16:
                vm->fp[a] = I16_VALUE(-AS_I16(vm->fp[b]));
                break;
            case OP_NEG_I8:
                vm->fp[a] = I8_VALUE(-AS_I8(vm->fp[b]));
                break;
            case OP_EQ:
                vm->fp[a] = BOOL_VALUE(AS_UNSIGNED(vm->fp[b]) == AS_UNSIGNED(vm->fp[c]));
                break;
            case OP_NE:
                vm->fp[a] = BOOL_VALUE(AS_UNSIGNED(vm->fp[b]) != AS_UNSIGNED(vm->fp[c]));
                break;
            case OP_LT_INT:
                vm->fp[a] = BOOL_VALUE(AS_SIGNED(vm->fp[b]) < AS_SIGNED(vm->fp[c]));
                break;
            case OP_LT_UINT:
                vm->fp[a] = BOOL_VALUE(AS_UNSIGNED(vm->fp[b]) < AS_UNSIGNED(vm->fp[c]));
                break;
            case OP_LE_INT:
                vm->fp[a] = BOOL_VALUE(AS_SIGNED(vm->fp[b]) <= AS_SIGNED(vm->fp[c]));
                break;
            case OP_LE_UINT:
                vm->fp[a] = BOOL_VALUE(AS_UNSIGNED(vm->fp[b]) <= AS_UNSIGNED(vm->fp[c]));
                break;
            case OP_BZ:
                if (!AS_UNSIGNED(vm->fp[a])) {
                    vm->ip += (int16_t)OPERAND_BC(inst);
                }
                break;
            case OP_BNZ:
                if (AS_UNSIGNED(vm->fp[a])) {
                    vm->ip += (int16_t)OPERAND_BC(inst);
                }
                break;
            case OP_BEQ:
                if (AS_UNSIGNED(vm->fp[a]) == AS_UNSIGNED(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BNE:
                if (AS_UNSIGNED(vm->fp[a]) != AS_UNSIGNED(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BLT_INT:
                if (AS_SIGNED(vm->fp[a]) < AS_SIGNED(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BLT_UINT:
                if (AS_UNSIGNED(vm->fp[a]) < AS_UNSIGNED(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BLE_INT:
                if (AS_SIGNED(vm->fp[a]) <= AS_SIGNED(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BLE_UINT:
                if (AS_UNSIGNED(vm->fp[a]) <= AS_UNSIGNED(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_JMP:
                vm->ip += SIGNED_OPERAND_ABC(inst);
                break;
            case OP_CALL:
                functionPosition = OPERAND_BC(inst);

                call: {
                    Value* newFrame = vm->fp + a;
                    function = vm->module->functions.data[functionPosition];
                    function->entry(vm, function, newFrame);
                break;
            }
            case OP_RET: {
                Value* frame = vm->fp;
                
                vm->ip = AS_POINTER(frame[-2]);
                vm->fp = AS_POINTER(frame[-1]);
                break;
            }
            case OP_RETV: {
                Value* frame = vm->fp;
                Instruction* ip = AS_POINTER(frame[-2]);
                Value* fp = AS_POINTER(frame[-1]);

                frame[-2] = frame[a];
                vm->ip = ip;
                vm->fp = fp;
                break;
            }
            case OP_CALL2: {
                Value* newFrame = vm->fp + a;

                function = vm->module->functions.data[b];
                function->entry(vm, function, newFrame);
                function = vm->module->functions.data[c];
                function->entry(vm, function, newFrame + 1);
                break;
            }
            case OP_LDI2:
                vm->fp[a] = SIGNED_VALUE((int8_t)b);
                vm->fp[a + 1] = SIGNED_VALUE((int8_t)c);
                break;
            case OP_LDG2:
                vm->fp[a] = vm->gp[b];
                vm->fp[a + 1] = vm->gp[c];
                break;
            case OP_MOV2:
                vm->fp[a] = vm->fp[b];
                vm->fp[a + 1] = vm->fp[c];
                break;
            case OP_LDI_LDG:
                vm->fp[a] = SIGNED_VALUE((int8_t)b);
                vm->fp[a + 1] = vm->gp[c];
                break;
            case OP_LDG_LDI:
                vm->fp[a] = vm->gp[b];
                vm->fp[a + 1] = SIGNED_VALUE((int8_t)c);
                break;
            case OP_LDI_CALL: {
                uint16_t operands = OPERAND_BC(inst);
                int16_t imm = LOAD_CALL_SIGNED_OPERAND(operands);

                vm->fp[a] = SIGNED_VALUE(imm);
                functionPosition = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OP_LDC_CALL: {
                uint16_t operands = OPERAND_BC(inst);

                vm->fp[a] = vm->module->constants.data[LOAD_CALL_OPERAND(operands)];
                functionPosition = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OP_LDG_CALL: {
                uint16_t operands = OPERAND_BC(inst);

                vm->fp[a] = vm->gp[LOAD_CALL_OPERAND(operands)];
                functionPosition = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OP_LDR_CALL: {
                uint16_t operands = OPERAND_BC(inst);

                vm->fp[a] = *(Value*)AS_POINTER(vm->fp[LOAD_CALL_OPERAND(operands)]);
                functionPosition = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OP_LDI_STG:
                vm->fp[a] = SIGNED_VALUE((int8_t)b);
                vm->gp[c] = vm->fp[a];
                break;
            default:
                return;
        }
    }
}

void initVM(VM* vm, ModuleObject* module)
{
    vm->module = module;
    vm->ip = NULL;
    vm->fp = vm->stack;
    vm->gp = vm->stack;
}

void freeVM(VM* vm)
{
    (void)vm;
}

void interpret(VM* vm)
{
    if (vm->module) {
        run(vm);
    }
}
