#include "vm/vm.h"
#include "common/errors.h"
#include "runtime/function_object.h"
#include "vm/opcode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX == UINT32_MAX
#define VM_WIDE_INSTRUCTIONS "vm/wide_32.inc"
#elif UINTPTR_MAX == UINT64_MAX
#define VM_WIDE_INSTRUCTIONS "vm/wide_64.inc"
#else
#error Unsupported pointer width
#endif

static _Noreturn void division_by_zero_error(void)
{
    fatal_error("Division by zero");
}

static _Noreturn void stack_overflow_error(void)
{
    fatal_error("Stack overflow");
}

static void test_frame_overflow(const VM* vm, const Value* frame, int max_stack_count)
{
    if (frame - vm->stack - 2 + max_stack_count > STACK_MAX) {
        stack_overflow_error();
    }
}

void enter_bytecode_function(VM* vm, const FunctionObject* function, Value* frame)
{
    test_frame_overflow(vm, frame, function->max_stack_count);

    frame[-2] = POINTER_VALUE(vm->ip);
    frame[-1] = POINTER_VALUE(vm->fp);
    vm->fp = frame;
    vm->ip = (Instruction*)function->code.data;
}

static void run(VM* vm)
{
    uint16_t function_position = 0;
    FunctionObject* function = vm->module->functions.data[function_position];
    vm->ip = (Instruction*)function->code.data;
    
    if (function->max_stack_count > STACK_MAX) {
        stack_overflow_error();
    }

    for (;;) {
        Instruction inst = *vm->ip++;
        Opcode opcode = (Opcode)OPCODE(inst);
        uint16_t a = OPERAND_A(inst);
        uint16_t b = OPERAND_B(inst);
        uint16_t c = OPERAND_C(inst);

        dispatch:
        switch (opcode) {
            case OpHlt:
                return;
            case OpNop:
                break;
            case OpMov:
                vm->fp[a] = vm->fp[b];
                break;
            case OpLdi:
                vm->fp[a] = SIGNED_VALUE((int16_t)OPERAND_BC(inst));
                break;
            case OpLdc:
                vm->fp[a] = vm->module->constants.data[OPERAND_BC(inst)];
                break;
            case OpLdg:
                vm->fp[a] = vm->globals.data[OPERAND_BC(inst)];
                break;
            case OpStg:
                vm->globals.data[OPERAND_BC(inst)] = vm->fp[a];
                break;
            case OpStgi:
                vm->fp[a] = SIGNED_VALUE((int8_t)b);
                vm->globals.data[c] = vm->fp[a];
                break;
            case OpRefl:
                vm->fp[a] = POINTER_VALUE(&vm->fp[b]);
                break;
            case OpRefg:
                vm->fp[a] = POINTER_VALUE(&vm->globals.data[OPERAND_BC(inst)]);
                break;
            case OpLdr:
                vm->fp[a] = *(Value*)AS_POINTER(vm->fp[b]);
                break;
            case OpStr:
                *(Value*)AS_POINTER(vm->fp[b]) = vm->fp[a];
                break;
#include VM_WIDE_INSTRUCTIONS
            case OpSextI32: {
                uint32_t value = AS_U32(vm->fp[b]);
                int32_t result = (int32_t)(value << (32 - c)) >> (32 - c);
                vm->fp[a] = I32_VALUE(result);
                break;
            }
            case OpZextI32: {
                uint32_t value = AS_U32(vm->fp[b]);
                uint32_t result = value & (UINT32_MAX >> (32 - c));
                vm->fp[a] = U32_VALUE(result);
                break;
            }
            case OpF32ToI32:
                vm->fp[a] = I32_VALUE((int32_t)AS_F32(vm->fp[b]));
                break;
            case OpF32ToU32:
                vm->fp[a] = U32_VALUE((uint32_t)AS_F32(vm->fp[b]));
                break;
            case OpI32ToF32:
                vm->fp[a] = F32_VALUE((float)AS_SIGNED(vm->fp[b]));
                break;
            case OpU32ToF32:
                vm->fp[a] = F32_VALUE((float)AS_UNSIGNED(vm->fp[b]));
                break;
            case OpAddF32: {
                float result = AS_F32(vm->fp[b]) + AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OpSubF32: {
                float result = AS_F32(vm->fp[b]) - AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OpMulF32: {
                float result = AS_F32(vm->fp[b]) * AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OpDivF32: {
                float result = AS_F32(vm->fp[b]) / AS_F32(vm->fp[c]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OpRemF32: {
                float left = AS_F32(vm->fp[b]);
                float right = AS_F32(vm->fp[c]);
                float result = fmodf(left, right);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OpNegF32: {
                float result = -AS_F32(vm->fp[b]);
                vm->fp[a] = F32_VALUE(result);
                break;
            }
            case OpAddI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) + AS_U32(vm->fp[c]));
                break;
            case OpAddiI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) + (int8_t)c);
                break;
            case OpAddI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) + AS_U16(vm->fp[c]));
                break;
            case OpAddiI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) + (int8_t)c);
                break;
            case OpAddI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) + AS_U8(vm->fp[c]));
                break;
            case OpAddiI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) + (int8_t)c);
                break;
            case OpSubI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) - AS_U32(vm->fp[c]));
                break;
            case OpSubiI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) - (int8_t)c);
                break;
            case OpSubI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) - AS_U16(vm->fp[c]));
                break;
            case OpSubiI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) - (int8_t)c);
                break;
            case OpSubI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) - AS_U8(vm->fp[c]));
                break;
            case OpSubiI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) - (int8_t)c);
                break;
            case OpMulI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) * AS_U32(vm->fp[c]));
                break;
            case OpMuliI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) * (int8_t)c);
                break;
            case OpMulI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) * AS_U16(vm->fp[c]));
                break;
            case OpMuliI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) * (int8_t)c);
                break;
            case OpMulI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) * AS_U8(vm->fp[c]));
                break;
            case OpMuliI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) * (int8_t)c);
                break;
            case OpIdivI32: {
                if (!AS_I32(vm->fp[c])) {
                    division_by_zero_error();
                }

                if (AS_I32(vm->fp[b]) == INT32_MIN && AS_I32(vm->fp[c]) == -1) {
                    vm->fp[a] = I32_VALUE(INT32_MIN);
                    break;
                }

                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) / AS_I32(vm->fp[c]));
                break;
            }
            case OpIdivI16: {
                if (!AS_I16(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) / AS_I16(vm->fp[c]));
                break;
            }
            case OpIdivI8: {
                if (!AS_I8(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) / AS_I8(vm->fp[c]));
                break;
            }
            case OpIdivU32: {
                if (!AS_U32(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) / AS_U32(vm->fp[c]));
                break;
            }
            case OpIdivU16: {
                if (!AS_U16(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) / AS_U16(vm->fp[c]));
                break;
            }
            case OpIdivU8: {
                if (!AS_U8(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) / AS_U8(vm->fp[c]));
                break;
            }
            case OpRemI32: {
                if (!AS_I32(vm->fp[c])) {
                    division_by_zero_error();
                }

                if (AS_I32(vm->fp[b]) == INT32_MIN && AS_I32(vm->fp[c]) == -1) {
                    vm->fp[a] = I32_VALUE(0);
                    break;
                }

                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) % AS_I32(vm->fp[c]));
                break;
            }
            case OpRemI16: {
                if (!AS_I16(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) % AS_I16(vm->fp[c]));
                break;
            }
            case OpRemI8: {
                if (!AS_I8(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) % AS_I8(vm->fp[c]));
                break;
            }
            case OpRemU32: {
                if (!AS_U32(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) % AS_U32(vm->fp[c]));
                break;
            }
            case OpRemU16: {
                if (!AS_U16(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) % AS_U16(vm->fp[c]));
                break;
            }
            case OpRemU8: {
                if (!AS_U8(vm->fp[c])) {
                    division_by_zero_error();
                }

                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) % AS_U8(vm->fp[c]));
                break;
            }
            case OpBandI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) & AS_U32(vm->fp[c]));
                break;
            case OpBandiI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) & c);
                break;
            case OpBandI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) & AS_U16(vm->fp[c]));
                break;
            case OpBandiI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) & c);
                break;
            case OpBandI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) & AS_U8(vm->fp[c]));
                break;
            case OpBandiI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) & c);
                break;
            case OpBorI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) | AS_U32(vm->fp[c]));
                break;
            case OpBoriI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) | c);
                break;
            case OpBorI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) | AS_U16(vm->fp[c]));
                break;
            case OpBoriI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) | c);
                break;
            case OpBorI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) | AS_U8(vm->fp[c]));
                break;
            case OpBoriI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) | c);
                break;
            case OpBxorI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) ^ AS_U32(vm->fp[c]));
                break;
            case OpBxoriI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) ^ c);
                break;
            case OpBxorI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) ^ AS_U16(vm->fp[c]));
                break;
            case OpBxoriI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) ^ c);
                break;
            case OpBxorI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) ^ AS_U8(vm->fp[c]));
                break;
            case OpBxoriI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) ^ c);
                break;
            case OpBnotI32:
                vm->fp[a] = I32_VALUE(~AS_U32(vm->fp[b]));
                break;
            case OpBnotI16:
                vm->fp[a] = I16_VALUE(~AS_U16(vm->fp[b]));
                break;
            case OpBnotI8:
                vm->fp[a] = I8_VALUE(~AS_U8(vm->fp[b]));
                break;
            case OpLslI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) << AS_U32(vm->fp[c]));
                break;
            case OpLsliI32:
                vm->fp[a] = I32_VALUE(AS_U32(vm->fp[b]) << c);
                break;
            case OpLslI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) << AS_U16(vm->fp[c]));
                break;
            case OpLsliI16:
                vm->fp[a] = I16_VALUE(AS_U16(vm->fp[b]) << c);
                break;
            case OpLslI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) << AS_U8(vm->fp[c]));
                break;
            case OpLsliI8:
                vm->fp[a] = I8_VALUE(AS_U8(vm->fp[b]) << c);
                break;
            case OpLsrI32:
                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) >> AS_U32(vm->fp[c]));
                break;
            case OpLsriI32:
                vm->fp[a] = U32_VALUE(AS_U32(vm->fp[b]) >> c);
                break;
            case OpLsrI16:
                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) >> AS_U16(vm->fp[c]));
                break;
            case OpLsriI16:
                vm->fp[a] = U16_VALUE(AS_U16(vm->fp[b]) >> c);
                break;
            case OpLsrI8:
                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) >> AS_U8(vm->fp[c]));
                break;
            case OpLsriI8:
                vm->fp[a] = U8_VALUE(AS_U8(vm->fp[b]) >> c);
                break;
            case OpAsrI32:
                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) >> AS_I32(vm->fp[c]));
                break;
            case OpAsriI32:
                vm->fp[a] = I32_VALUE(AS_I32(vm->fp[b]) >> c);
                break;
            case OpAsrI16:
                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) >> AS_I16(vm->fp[c]));
                break;
            case OpAsriI16:
                vm->fp[a] = I16_VALUE(AS_I16(vm->fp[b]) >> c);
                break;
            case OpAsrI8:
                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) >> AS_I8(vm->fp[c]));
                break;
            case OpAsriI8:
                vm->fp[a] = I8_VALUE(AS_I8(vm->fp[b]) >> c);
                break;
            case OpAnd:
                vm->fp[a] = BOOL_VALUE(AS_BOOL(vm->fp[b]) && AS_BOOL(vm->fp[c]));
                break;
            case OpOr:
                vm->fp[a] = BOOL_VALUE(AS_BOOL(vm->fp[b]) || AS_BOOL(vm->fp[c]));
                break;
            case OpNot:
                vm->fp[a] = SIGNED_VALUE(!AS_SIGNED(vm->fp[b]));
                break;
            case OpNegI32:
                vm->fp[a] = SIGNED_VALUE(-AS_SIGNED(vm->fp[b]));
                break;
            case OpNegI16:
                vm->fp[a] = I16_VALUE(-AS_I16(vm->fp[b]));
                break;
            case OpNegI8:
                vm->fp[a] = I8_VALUE(-AS_I8(vm->fp[b]));
                break;
            case OpCmpI32: {
                int32_t left = AS_I32(vm->fp[b]);
                int32_t right = AS_I32(vm->fp[c]);
                int32_t result = (left > right) - (left < right);

                vm->fp[a] = I32_VALUE(result);
                break;
            }
            case OpCmpU32: {
                uint32_t left = AS_U32(vm->fp[b]);
                uint32_t right = AS_U32(vm->fp[c]);
                int32_t result = (left > right) - (left < right);

                vm->fp[a] = I32_VALUE(result);
                break;
            }
            case OpCmplF32:
            case OpCmpgF32: {
                float left = AS_F32(vm->fp[b]);
                float right = AS_F32(vm->fp[c]);
                int32_t result = (left > right) - (left < right);

                if (isnan(left) || isnan(right)) {
                    result = opcode == OpCmplF32 ? -1 : 1;
                }

                vm->fp[a] = I32_VALUE(result);
                break;
            }
            case OpEq:
                vm->fp[a] = BOOL_VALUE(AS_U32(vm->fp[b]) == AS_U32(vm->fp[c]));
                break;
            case OpEqi:
                vm->fp[a] = BOOL_VALUE(AS_I32(vm->fp[b]) == (int8_t)c);
                break;
            case OpNe:
                vm->fp[a] = BOOL_VALUE(AS_U32(vm->fp[b]) != AS_U32(vm->fp[c]));
                break;
            case OpNei:
                vm->fp[a] = BOOL_VALUE(AS_I32(vm->fp[b]) != (int8_t)c);
                break;
            case OpLtI32:
                vm->fp[a] = BOOL_VALUE(AS_I32(vm->fp[b]) < AS_I32(vm->fp[c]));
                break;
            case OpLtU32:
                vm->fp[a] = BOOL_VALUE(AS_U32(vm->fp[b]) < AS_U32(vm->fp[c]));
                break;
            case OpLeI32:
                vm->fp[a] = BOOL_VALUE(AS_I32(vm->fp[b]) <= AS_I32(vm->fp[c]));
                break;
            case OpLeU32:
                vm->fp[a] = BOOL_VALUE(AS_U32(vm->fp[b]) <= AS_U32(vm->fp[c]));
                break;
            case OpBz:
                if (!AS_U32(vm->fp[a])) {
                    vm->ip += (int16_t)OPERAND_BC(inst);
                }
                break;
            case OpBnz:
                if (AS_U32(vm->fp[a])) {
                    vm->ip += (int16_t)OPERAND_BC(inst);
                }
                break;
            case OpBeq:
                if (AS_U32(vm->fp[a]) == AS_U32(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBeqi:
                if (AS_I32(vm->fp[a]) == (int8_t)b) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBne:
                if (AS_U32(vm->fp[a]) != AS_U32(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBnei:
                if (AS_I32(vm->fp[a]) != (int8_t)b) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBltI32:
                if (AS_I32(vm->fp[a]) < AS_I32(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBltU32:
                if (AS_U32(vm->fp[a]) < AS_U32(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBleI32:
                if (AS_I32(vm->fp[a]) <= AS_I32(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpBleU32:
                if (AS_U32(vm->fp[a]) <= AS_U32(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OpJmp:
                vm->ip += SIGNED_OPERAND_ABC(inst);
                break;
            case OpCall:
                function_position = OPERAND_BC(inst);

                call: {
                    Value* new_frame = vm->fp + a;
                    function = vm->module->functions.data[function_position];
                    function->entry(vm, function, new_frame);
                break;
            }
            case OpRet: {
                Value* frame = vm->fp;
                
                vm->ip = AS_POINTER(frame[-2]);
                vm->fp = AS_POINTER(frame[-1]);
                break;
            }
            case OpRetv: {
                Value* frame = vm->fp;
                Instruction* ip = AS_POINTER(frame[-2]);
                Value* fp = AS_POINTER(frame[-1]);

                frame[-2] = frame[a];
                vm->ip = ip;
                vm->fp = fp;
                break;
            }
            case OpCallCall: {
                Value* new_frame = vm->fp + a;

                function = vm->module->functions.data[b];
                function->entry(vm, function, new_frame);
                function = vm->module->functions.data[c];
                function->entry(vm, function, new_frame + 1);
                break;
            }
            case OpLdiLdi:
                vm->fp[a] = SIGNED_VALUE((int8_t)b);
                vm->fp[a + 1] = SIGNED_VALUE((int8_t)c);
                break;
            case OpLdcLdc:
                vm->fp[a] = vm->module->constants.data[b];
                vm->fp[a + 1] = vm->module->constants.data[c];
                break;
            case OpLdgLdg:
                vm->fp[a] = vm->globals.data[b];
                vm->fp[a + 1] = vm->globals.data[c];
                break;
            case OpMovMov:
                vm->fp[a] = vm->fp[b];
                vm->fp[a + 1] = vm->fp[c];
                break;
            case OpMovLdi:
                vm->fp[a] = vm->fp[b];
                vm->fp[a + 1] = SIGNED_VALUE((int8_t)c);
                break;
            case OpMovLdc:
                vm->fp[a] = vm->fp[b];
                vm->fp[a + 1] = vm->module->constants.data[c];
                break;
            case OpMovLdg:
                vm->fp[a] = vm->fp[b];
                vm->fp[a + 1] = vm->globals.data[c];
                break;
            case OpLdiLdg:
                vm->fp[a] = SIGNED_VALUE((int8_t)b);
                vm->fp[a + 1] = vm->globals.data[c];
                break;
            case OpLdgLdi:
                vm->fp[a] = vm->globals.data[b];
                vm->fp[a + 1] = SIGNED_VALUE((int8_t)c);
                break;
            case OpStgCall:
                vm->globals.data[b] = vm->fp[a];
                function_position = c;
                goto call;
            case OpLdiCall: {
                uint16_t operands = OPERAND_BC(inst);
                int16_t imm = LOAD_CALL_SIGNED_OPERAND(operands);

                vm->fp[a] = SIGNED_VALUE(imm);
                function_position = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OpLdcCall: {
                uint16_t operands = OPERAND_BC(inst);

                vm->fp[a] = vm->module->constants.data[LOAD_CALL_OPERAND(operands)];
                function_position = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OpLdgCall: {
                uint16_t operands = OPERAND_BC(inst);

                vm->fp[a] = vm->globals.data[LOAD_CALL_OPERAND(operands)];
                function_position = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OpLdrCall: {
                uint16_t operands = OPERAND_BC(inst);

                vm->fp[a] = *(Value*)AS_POINTER(vm->fp[LOAD_CALL_OPERAND(operands)]);
                function_position = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OpWide: {
                Instruction prefix = inst;
                inst = *vm->ip++;
                opcode = (Opcode)OPCODE(inst);
                a = ((uint16_t)OPERAND_A(prefix) << 8) | OPERAND_A(inst);
                b = ((uint16_t)OPERAND_B(prefix) << 8) | OPERAND_B(inst);
                c = ((uint16_t)OPERAND_C(prefix) << 8) | OPERAND_C(inst);

                goto dispatch;
            }
            default:
                return;
        }
    }
}

void init_vm(VM* vm, ModuleObject* module)
{
    vm->module = module;
    vm->ip = NULL;
    vm->fp = vm->stack;
    vm->exit_requested = false;
    vm->exit_status = EXIT_SUCCESS;

    init_value_array(&vm->globals);
    resize_value_array(&vm->globals, module->global_count);
}

void free_vm(VM* vm)
{
    free_value_array(&vm->globals);
}

int interpret(VM* vm)
{
    vm->exit_requested = false;
    vm->exit_status = EXIT_SUCCESS;
    vm->fp = vm->stack;

    if (!vm->module) {
        return vm->exit_status;
    }

    if (setjmp(vm->exit_jump) != 0) {
        return vm->exit_status;
    }

    run(vm);

    return vm->exit_status;
}

_Noreturn void exit_vm(VM* vm, int status)
{
    vm->exit_status = status;
    vm->exit_requested = true;

    longjmp(vm->exit_jump, 1);
}
