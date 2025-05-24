#include "vm.h"
#include "bytecode.h"
#include "codeobject.h"
#include "compiler.h"
#include "config.h"
#include "functionobject.h"
#include "moduleobject.h"
#include "value.h"
#include "service.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUSH(value) (vm.stackTop[0] = value, vm.stackTop++)
#define POP() ((--vm.stackTop)[0])
#define READ_UINT16() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_UINT8() ((uint8_t)*(frame->ip++))
#define TEST_OVERFLOW(fn) \
    if (vm.stackTop - vm.stack + fn->maxStackCount > STACK_MAX) error(stackOverflowError)

typedef void (*service_t)();

typedef struct StackFrame
{
    uint8_t* ip;
    Value* slots;
} StackFrame;

typedef struct VM
{
    StackFrame frames[FRAMES_MAX];
    service_t service[SERVICES_MAX];
    Value stack[STACK_MAX];
    Value* stackTop;
    ValueArray globals;
    size_t frameCount;
    ModuleObject* module;
} VM;

static VM vm;

static const char* stackOverflowError = "Error: Stack overflow\n";

static void error(const char* message)
{
    fprintf(stderr, message);
    exit(1);
}

static void sys_exit()
{
    exit(0);
}

static void sys_print()
{
    int32_t n = AS_INT(POP());
    
    printf("%d\n", n);
    PUSH(INT_VALUE(0));
}

static void sys_clamp()
{
    int32_t num = AS_INT(POP());
    int32_t min = AS_INT(POP());
    int32_t max = AS_INT(POP());

    if (num < min) {
        PUSH(INT_VALUE(min));
    } else if (num > max) {
        PUSH(INT_VALUE(max));
    } else {
        PUSH(INT_VALUE(num));
    }
}

static void sys_abs()
{
    int32_t n = AS_INT(POP());

    PUSH(INT_VALUE(n < 0 ? -n : n));
}

static void sys_min()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a < b ? a : b));
}

static void sys_max()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a > b ? a : b));
}

static void sys_byteorder()
{
    int32_t i = 1;
    char* c = (char*)&i;
    Value value = INT_VALUE((int32_t)*c);

    PUSH(value);
}

static void initServices()
{
    vm.service[SYS_EXIT] = sys_exit;
    vm.service[SYS_PRINT] = sys_print;
    vm.service[SYS_CLAMP] = sys_clamp;
    vm.service[SYS_ABS] = sys_abs;
    vm.service[SYS_MIN] = sys_min;
    vm.service[SYS_MAX] = sys_max;
    vm.service[SYS_BYTEORDER] = sys_byteorder;
}

static void run()
{
    uint8_t opcode;
    FunctionObject* function = AS_POINTER(vm.module->constants.data[0]);
    StackFrame* frame = &vm.frames[vm.frameCount - 1];
    int32_t a;
    int32_t b;
    int32_t x;
    Value value;

    frame->ip = function->code.data;
    
    TEST_OVERFLOW(function);

    while (opcode = READ_UINT8()) {
        switch (opcode)
        {
            case OP_SYSCALL:
                x = READ_UINT8();
                vm.service[x]();
                break;

            case OP_LDC:
                x = READ_UINT8();
                value = vm.module->constants.data[x];
                PUSH(value);
                break;

            case OP_REG:
                pushValue(&vm.globals, POP());
                break;

            case OP_LDG:
                x = READ_UINT8();
                value = vm.globals.data[x];
                PUSH(value);
                break;

            case OP_STG:
                x = READ_UINT8();
                vm.globals.data[x] = POP();
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
                PUSH(INT_VALUE(x));
                break;

            case OP_PUSHH:
                x = (int16_t) READ_UINT16();
                PUSH(INT_VALUE(x));
                break;

            case OP_PUSH_0:
                PUSH(INT_VALUE(0));
                break;

            case OP_PUSH_1:
                PUSH(INT_VALUE(1));
                break;

            case OP_PUSH_2:
                PUSH(INT_VALUE(2));
                break;

            case OP_PUSH_3:
                PUSH(INT_VALUE(3));
                break;

            case OP_POP:
                POP();
                break;

            case OP_DUP:
                PUSH(vm.stackTop[-1]);
                break;

            case OP_INC:
                AS_INT(vm.stackTop[-1])++;
                break;

            case OP_DEC:
                AS_INT(vm.stackTop[-1])--;
                break;
            
            case OP_ADD:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a + b));
                break;

            case OP_SUB:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a - b));
                break;

            case OP_MUL:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a * b));
                break;

            case OP_DIV:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a / b));
                break;

            case OP_REM:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a % b));
                break;

            case OP_POW:
                b = AS_INT(POP());
                a = AS_INT(POP());
                x = pow(a, b);
                PUSH(INT_VALUE(x));
                break;

            case OP_BAND:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a & b));
                break;

            case OP_BOR:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a | b));
                break;

            case OP_BXOR:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a ^ b));
                break;

            case OP_BNOT:
                x = AS_INT(vm.stackTop[-1]);
                vm.stackTop[-1] = INT_VALUE(~x);
                break;

            case OP_LSL:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a << b));
                break;

            case OP_LSR:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(a >> b));
                break;

            case OP_ASR:
                b = AS_INT(POP());
                a = AS_INT(POP());
                PUSH(INT_VALUE(~(~a >> b)));
                break;

            case OP_NEG:
                x = AS_INT(vm.stackTop[-1]);
                vm.stackTop[-1] = INT_VALUE(-x);
                break;

            case OP_NOT:
                x = AS_INT(vm.stackTop[-1]);
                vm.stackTop[-1] = INT_VALUE(!x);
                break;

            case OP_BEQ:
                b = AS_INT(vm.stackTop[-1]);
                a = AS_INT(vm.stackTop[-2]);
                if (a == b) frame->ip += READ_UINT16();
                break;

            case OP_BLT:
                b = AS_INT(vm.stackTop[-1]);
                a = AS_INT(vm.stackTop[-2]);
                if (a < b) frame->ip += READ_UINT16();
                break;

            case OP_BLE:
                b = AS_INT(vm.stackTop[-1]);
                a = AS_INT(vm.stackTop[-2]);
                if (a <= b) frame->ip += READ_UINT16();
                break;

            case OP_JMP:
                frame->ip += READ_UINT16();
                break;

            case OP_CALL:
                x = READ_UINT16();
                FunctionObject* function = AS_POINTER(vm.module->constants.data[x]);
                
                TEST_OVERFLOW(function);

                frame = &vm.frames[vm.frameCount++];
                frame->ip = function->code.data;
                frame->slots = vm.stackTop - function->paramCount;
                break;

            case OP_RET:
                vm.stackTop = frame->slots;
                vm.frameCount--;
                frame = &vm.frames[vm.frameCount - 1];
                PUSH(INT_VALUE(0));
                break;

            case OP_RETV:
                value = POP();
                vm.stackTop = frame->slots;
                vm.frameCount--;
                frame = &vm.frames[vm.frameCount - 1];
                PUSH(value);
                break;

            default:
                return;
        }
    }
}

void initVM(ModuleObject* module)
{
    initValueArray(&vm.globals);
    initServices();
    
    vm.module = module;
    vm.stackTop = vm.stack;
    vm.frameCount = 0;

    StackFrame* frame = &vm.frames[vm.frameCount++];
    frame->ip = NULL;
    frame->slots = vm.stack;
}

void freeVM()
{
    freeValueArray(&vm.globals);
}

void inspectStack()
{
    for (int i = 0; i < STACK_MAX; i++) {
        char* arrow = &vm.stack[i] == vm.stackTop ? " <-" : "";
        int n = AS_INT(vm.stack[i]);

        printf("%d: %d%s\n", i, n, arrow);
    }
}

void interpret()
{
    if (!vm.module) {
        return;
    }

    run();
}

#undef READ_UINT8
#undef READ_UINT16
