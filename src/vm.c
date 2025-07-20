#include "vm.h"
#include "builtin.h"
#include "bytecode.h"
#include "codeobject.h"
#include "compiler.h"
#include "config.h"
#include "functionobject.h"
#include "moduleobject.h"
#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PUSH(value) ((vm->stackTop++)[0] = value)
#define PUSH_BOOL(i) (PUSH(BOOL_VALUE(i)))
#define PUSH_INT(i) (PUSH(INT_VALUE(i)))
#define PUSH_FLOAT(i) (PUSH(FLOAT_VALUE(i)))
#define PUSH_POINTER(i) (PUSH(POINTER_VALUE(i)))

#define POP() ((--vm->stackTop)[0])
#define POP_BOOL() (AS_BOOL(POP()))
#define POP_INT() (AS_INT(POP()))
#define POP_FLOAT() (AS_FLOAT(POP()))
#define POP_POINTER() (AS_POINTER(POP()))

#define READ_UINT16() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_UINT8() ((uint8_t)*(frame->ip++))

#define TEST_OVERFLOW(n) \
    if (vm->stackTop - vm->stack + n > STACK_MAX) error("Error: Stack overflow\n")

static void error(const char* message)
{
    fprintf(stderr, message);
    exit(1);
}

static void initServices(VM* vm)
{
    vm->service[SOP_EXIT] = __exit;
    vm->service[SOP_PRINT] = __print;
    vm->service[SOP_CLAMP] = __clamp;
    vm->service[SOP_ABS] = __abs;
    vm->service[SOP_MIN] = __min;
    vm->service[SOP_MAX] = __max;
    vm->service[SOP_BYTEORDER] = __byteorder;
}

static void run(VM* vm)
{
    uint8_t opcode;
    FunctionObject* function = AS_POINTER(vm->module->constants.data[0]);
    StackFrame* frame = &vm->frames[vm->frameCount - 1];
    int32_t a;
    int32_t b;
    int32_t x;
    Value value;

    frame->ip = function->code.data;
    
    TEST_OVERFLOW(function->maxStackCount);

    while (opcode = READ_UINT8()) {
        switch (opcode)
        {
            case OP_REQ:
                x = READ_UINT8();
                Service service = services[x];
                Value result = vm->service[x](service.paramCount, vm->stackTop - service.paramCount);
                vm->stackTop -= service.paramCount;
                PUSH(result);
                break;

            case OP_LDC:
                x = READ_UINT8();
                value = vm->module->constants.data[x];
                PUSH(value);
                break;

            case OP_REG:
                pushValue(&vm->globals, POP());
                break;

            case OP_LDG:
                x = READ_UINT8();
                value = vm->globals.data[x];
                PUSH(value);
                break;

            case OP_STG:
                x = READ_UINT8();
                vm->globals.data[x] = POP();
                break;

            case OP_LDL:
                x = READ_UINT8();
                PUSH(frame->slots[x]);
                break;

            case OP_LDL_0:
                PUSH(frame->slots[0]);
                break;

            case OP_LDL_1:
                PUSH(frame->slots[1]);
                break;

            case OP_LDL_2:
                PUSH(frame->slots[2]);
                break;

            case OP_LDL_3:
                PUSH(frame->slots[3]);
                break;

            case OP_STL:
                x = READ_UINT8();
                frame->slots[x] = POP();
                break;

            case OP_STL_0:
                frame->slots[0] = POP();
                break;

            case OP_STL_1:
                frame->slots[1] = POP();
                break;

            case OP_STL_2:
                frame->slots[2] = POP();
                break;

            case OP_STL_3:
                frame->slots[3] = POP();
                break;

            case OP_PUSHB:
                x = (int8_t) READ_UINT8();
                PUSH_INT(x);
                break;

            case OP_PUSHH:
                x = (int16_t) READ_UINT16();
                PUSH_INT(x);
                break;

            case OP_PUSH_0:
                PUSH_INT(0);
                break;

            case OP_PUSH_1:
                PUSH_INT(1);
                break;

            case OP_PUSH_2:
                PUSH_INT(2);
                break;

            case OP_PUSH_3:
                PUSH_INT(3);
                break;

            case OP_POP:
                vm->stackTop--;
                break;

            case OP_DUP:
                PUSH(vm->stackTop[-1]);
                break;

            case OP_INC:
                AS_INT(vm->stackTop[-1])++;
                break;

            case OP_DEC:
                AS_INT(vm->stackTop[-1])--;
                break;
            
            case OP_ADD:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a + b);
                break;

            case OP_SUB:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a - b);
                break;

            case OP_MUL:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a * b);
                break;

            case OP_DIV:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a / b);
                break;

            case OP_REM:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a % b);
                break;

            case OP_POW:
                b = POP_INT();
                a = POP_INT();
                x = pow(a, b);
                PUSH_INT(x);
                break;

            case OP_BAND:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a & b);
                break;

            case OP_BOR:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a | b);
                break;

            case OP_BXOR:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a ^ b);
                break;

            case OP_BNOT:
                x = AS_INT(vm->stackTop[-1]);
                vm->stackTop[-1] = INT_VALUE(~x);
                break;

            case OP_LSL:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a << b);
                break;

            case OP_LSR:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(a >> b);
                break;

            case OP_ASR:
                b = POP_INT();
                a = POP_INT();
                PUSH_INT(~(~a >> b));
                break;

            case OP_NEG:
                x = AS_INT(vm->stackTop[-1]);
                vm->stackTop[-1] = INT_VALUE(-x);
                break;

            case OP_NOT:
                x = AS_INT(vm->stackTop[-1]);
                vm->stackTop[-1] = INT_VALUE(!x);
                break;

            case OP_BEQ:
                b = AS_INT(vm->stackTop[-1]);
                a = AS_INT(vm->stackTop[-2]);
                if (a == b) frame->ip += READ_UINT16();
                break;

            case OP_BLT:
                b = AS_INT(vm->stackTop[-1]);
                a = AS_INT(vm->stackTop[-2]);
                if (a < b) frame->ip += READ_UINT16();
                break;

            case OP_BLE:
                b = AS_INT(vm->stackTop[-1]);
                a = AS_INT(vm->stackTop[-2]);
                if (a <= b) frame->ip += READ_UINT16();
                break;

            case OP_JMP:
                frame->ip += READ_UINT16();
                break;

            case OP_CALL:
                x = READ_UINT16();
                FunctionObject* function = AS_POINTER(vm->module->constants.data[x]);
                
                TEST_OVERFLOW(function->maxStackCount);

                frame = &vm->frames[vm->frameCount++];
                frame->ip = function->code.data;
                frame->slots = vm->stackTop - function->paramCount;
                break;

            case OP_RET:
                vm->stackTop = frame->slots;
                vm->frameCount--;
                frame = &vm->frames[vm->frameCount - 1];
                PUSH_INT(0);
                break;

            case OP_RETV:
                value = POP();
                vm->stackTop = frame->slots;
                vm->frameCount--;
                frame = &vm->frames[vm->frameCount - 1];
                PUSH(value);
                break;

            default:
                return;
        }
    }
}

void initVM(VM* vm, ModuleObject* module)
{
    initValueArray(&vm->globals);
    initServices(vm);
    
    vm->module = module;
    vm->stackTop = vm->stack;
    vm->frameCount = 0;

    StackFrame* frame = &vm->frames[vm->frameCount++];
    frame->ip = NULL;
    frame->slots = vm->stack;
}

void freeVM(VM* vm)
{
    freeValueArray(&vm->globals);
}

void inspectStack(VM* vm)
{
    for (int i = 0; i < STACK_MAX; i++) {
        char* arrow = &vm->stack[i] == vm->stackTop ? " <-" : "";
        int n = AS_INT(vm->stack[i]);

        printf("%d: %d%s\n", i, n, arrow);
    }
}

void interpret(VM* vm)
{
    if (!vm->module) {
        return;
    }

    run(vm);
}

#undef READ_UINT8
#undef READ_UINT16
