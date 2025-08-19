#include "vm.h"
#include "codeobject.h"
#include "functionobject.h"
#include "moduleobject.h"
#include "native.h"
#include "opcode.h"
#include "service.h"
#include "value.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUSH(value) ((vm->sp++)[0] = (value))
#define PUSH_BOOL(i) (PUSH(BOOL_VALUE(i)))
#define PUSH_FLOAT(i) (PUSH(FLOAT_VALUE(i)))
#define PUSH_INT(i) (PUSH(INT_VALUE(i)))
#define PUSH_POINTER(i) (PUSH(POINTER_VALUE(i)))

#define POP() ((--vm->sp)[0])
#define POP_BOOL() (AS_BOOL(POP()))
#define POP_FLOAT() (AS_FLOAT(POP()))
#define POP_INT() (AS_INT(POP()))
#define POP_POINTER() (AS_POINTER(POP()))

#define READ_UINT16() (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]))
#define READ_UINT8() ((uint8_t)*(vm->ip++))

#define TEST_OVERFLOW(n) if (vm->sp - vm->stack + (n) > STACK_MAX) \
    fprintf(stderr, "Error: Stack overflow\n"), \
    exit(1)

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
    int32_t a;
    int32_t b;
    int32_t x;
    Service service;
    Value value;

    vm->ip = function->code.data;
    
    TEST_OVERFLOW(function->maxStackCount);

    while ((opcode = READ_UINT8())) {
        switch (opcode) {
            case OP_REQS:
                x = READ_UINT8();
                service = services[x];
                value = vm->service[x](vm->sp - service.paramCount);
                vm->sp -= service.paramCount;
                PUSH(value);
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
                x = (int8_t) READ_UINT8();
                PUSH(vm->fp[x]);
                break;

            case OP_LDL_0:
                PUSH(vm->fp[0]);
                break;

            case OP_LDL_1:
                PUSH(vm->fp[1]);
                break;

            case OP_LDL_2:
                PUSH(vm->fp[2]);
                break;

            case OP_LDL_3:
                PUSH(vm->fp[3]);
                break;

            case OP_STL:
                x = (int8_t) READ_UINT8();
                vm->fp[x] = POP();
                break;

            case OP_STL_0:
                vm->fp[0] = POP();
                break;

            case OP_STL_1:
                vm->fp[1] = POP();
                break;

            case OP_STL_2:
                vm->fp[2] = POP();
                break;

            case OP_STL_3:
                vm->fp[3] = POP();
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
                vm->sp--;
                break;

            case OP_DUP:
                PUSH(vm->sp[-1]);
                break;

            case OP_INC:
                AS_INT(vm->sp[-1])++;
                break;

            case OP_DEC:
                AS_INT(vm->sp[-1])--;
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
                x = AS_INT(vm->sp[-1]);
                vm->sp[-1] = INT_VALUE(~x);
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
                x = AS_INT(vm->sp[-1]);
                vm->sp[-1] = INT_VALUE(-x);
                break;

            case OP_NOT:
                x = AS_INT(vm->sp[-1]);
                vm->sp[-1] = INT_VALUE(!x);
                break;

            case OP_BEQ:
                b = AS_INT(vm->sp[-1]);
                a = AS_INT(vm->sp[-2]);
                if (a == b) vm->ip += READ_UINT16();
                break;

            case OP_BLT:
                b = AS_INT(vm->sp[-1]);
                a = AS_INT(vm->sp[-2]);
                if (a < b) vm->ip += READ_UINT16();
                break;

            case OP_BLE:
                b = AS_INT(vm->sp[-1]);
                a = AS_INT(vm->sp[-2]);
                if (a <= b) vm->ip += READ_UINT16();
                break;

            case OP_JMP:
                vm->ip += READ_UINT16();
                break;

            case OP_CALL:
                x = READ_UINT16();
                function = AS_POINTER(vm->module->constants.data[x]);

                TEST_OVERFLOW(function->maxStackCount);
                PUSH_INT(function->paramCount);
                PUSH_POINTER(vm->ip);
                PUSH_POINTER(vm->fp);

                vm->ip = function->code.data;
                vm->fp = vm->sp;
                break;

            case OP_RET:
                vm->sp = vm->fp;
                vm->fp = POP_POINTER();
                vm->ip = POP_POINTER();
                vm->sp -= POP_INT();
                PUSH_INT(0);
                break;

            case OP_RETV:
                value = POP();
                vm->sp = vm->fp;
                vm->fp = POP_POINTER();
                vm->ip = POP_POINTER();
                vm->sp -= POP_INT();
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
    vm->ip = NULL;
    vm->sp = vm->stack;
    vm->fp = vm->stack;
}

void freeVM(VM* vm)
{
    freeValueArray(&vm->globals);
}

void inspectStack(VM* vm)
{
    for (int i = 0; i < STACK_MAX; i++) {
        char* arrow = &vm->stack[i] == vm->sp ? " <-" : "";
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
