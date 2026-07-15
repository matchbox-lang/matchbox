#include "vm.h"
#include "function_object.h"
#include "opcode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void stackOverflowError(void)
{
    fprintf(stderr, "Error: Stack overflow\n");
    exit(1);
}

static void initBuiltins(VM* vm)
{
    vm->builtins[BUILTIN_EXIT] = builtinExit;
    vm->builtins[BUILTIN_PRINT] = builtinPrint;
    vm->builtins[BUILTIN_CLAMP] = builtinClamp;
    vm->builtins[BUILTIN_ABS] = builtinAbs;
    vm->builtins[BUILTIN_MIN] = builtinMin;
    vm->builtins[BUILTIN_MAX] = builtinMax;
    vm->builtins[BUILTIN_BYTEORDER] = builtinByteorder;
}

static void testFrameOverflow(VM* vm, Value* frame, int maxStackCount)
{
    if (frame - vm->stack - 2 + maxStackCount > STACK_MAX) {
        stackOverflowError();
    }
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
            case OP_LDC:
                vm->fp[a] = vm->module->constants.data[OPERAND_BC(inst)];
                break;
            case OP_LDI:
                vm->fp[a] = INT_VALUE((int16_t)OPERAND_BC(inst));
                break;
            case OP_LDG:
                vm->fp[a] = vm->gp[OPERAND_BC(inst)];
                break;
            case OP_STG:
                vm->gp[OPERAND_BC(inst)] = vm->fp[a];
                break;
            case OP_ADD:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) + AS_INT(vm->fp[c]));
                break;
            case OP_SUB:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) - AS_INT(vm->fp[c]));
                break;
            case OP_MUL:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) * AS_INT(vm->fp[c]));
                break;
            case OP_DIV:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) / AS_INT(vm->fp[c]));
                break;
            case OP_REM:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) % AS_INT(vm->fp[c]));
                break;
            case OP_POW:
                vm->fp[a] = INT_VALUE(pow(AS_INT(vm->fp[b]), AS_INT(vm->fp[c])));
                break;
            case OP_BAND:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) & AS_INT(vm->fp[c]));
                break;
            case OP_BOR:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) | AS_INT(vm->fp[c]));
                break;
            case OP_BXOR:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) ^ AS_INT(vm->fp[c]));
                break;
            case OP_BNOT:
                vm->fp[a] = INT_VALUE(~AS_INT(vm->fp[b]));
                break;
            case OP_LSL:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) << AS_INT(vm->fp[c]));
                break;
            case OP_LSR:
                vm->fp[a] = INT_VALUE(AS_INT(vm->fp[b]) >> AS_INT(vm->fp[c]));
                break;
            case OP_ASR:
                vm->fp[a] = INT_VALUE(~(~AS_INT(vm->fp[b]) >> AS_INT(vm->fp[c])));
                break;
            case OP_NOT:
                vm->fp[a] = INT_VALUE(!AS_INT(vm->fp[b]));
                break;
            case OP_NEG:
                vm->fp[a] = INT_VALUE(-AS_INT(vm->fp[b]));
                break;
            case OP_BEQ:
                if (AS_INT(vm->fp[a]) == AS_INT(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BLT:
                if (AS_INT(vm->fp[a]) < AS_INT(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_BLE:
                if (AS_INT(vm->fp[a]) <= AS_INT(vm->fp[b])) {
                    vm->ip += (int8_t)c;
                }
                break;
            case OP_JMP:
                vm->ip += SIGNED_OPERAND_ABC(inst);
                break;
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
            case OP_LDI_CALL: {
                uint16_t operands = OPERAND_BC(inst);
                int16_t imm = LOAD_CALL_SIGNED_OPERAND(operands);

                vm->fp[a] = INT_VALUE(imm);
                functionPosition = LOAD_CALL_FUNCTION(operands);
                goto call;
            }
            case OP_CALL:
                functionPosition = OPERAND_BC(inst);

                call: {
                    Value* newFrame = vm->fp + a;
                    function = vm->module->functions.data[functionPosition];

                    if (function->type == FUNCTION_BUILTIN) {
                        vm->builtins[function->builtinId](newFrame);
                        break;
                    }

                    testFrameOverflow(vm, newFrame, function->maxStackCount);
                    newFrame[-2] = POINTER_VALUE(vm->ip);
                    newFrame[-1] = POINTER_VALUE(vm->fp);
                    vm->fp = newFrame;
                    vm->ip = (Instruction*)function->code.data;
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
                frame[0] = frame[a];
                vm->ip = AS_POINTER(frame[-2]);
                vm->fp = AS_POINTER(frame[-1]);
                break;
            }
            default:
                return;
        }
    }
}

void initVM(VM* vm, ModuleObject* module)
{
    initBuiltins(vm);

    vm->module = module;
    vm->ip = NULL;
    vm->sp = vm->stack;
    vm->fp = vm->stack;
    vm->gp = vm->stack;
}

void freeVM(VM* vm)
{
    (void)vm;
}

void inspectStack(VM* vm)
{
    for (int i = 0; i < STACK_MAX; i++) {
        printf("%d: %d\n", i, AS_INT(vm->stack[i]));
    }
}

void interpret(VM* vm)
{
    if (vm->module) {
        run(vm);
    }
}
