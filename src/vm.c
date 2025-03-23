#include "vm.h"
#include "bytecode.h"
#include "chunk.h"
#include "compiler.h"
#include "config.h"
#include "function.h"
#include "service.h"
#include "value.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_SIZE 1024
#define TEST_STACK_OVERFLOW(n) if (sp - stack + n > STACK_SIZE) error(stackOverflowError);
#define PUSH(value) (sp[0] = value, sp++)
#define POP() ((--sp)[0])
#define READ_UINT16() (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))
#define READ_UINT8() ((uint8_t)*(pc++))

typedef void (*service_t)();

static service_t service[SYS_SIZE];
static Value stack[STACK_SIZE];
static Value* sp;
static Value* fp;
static uint8_t* pc;
static uint8_t* bc;
static FunctionArray* functions;
static ValueArray* globals;
static ValueArray* constants;

static const char* stackOverflowError = "Error: Stack overflow\n";
extern CommandArgs cargs;

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

    sp[-1] = INT_VALUE((int32_t)*c);
}

static void initServices()
{
    service[SYS_EXIT] = sys_exit;
    service[SYS_PRINT] = sys_print;
    service[SYS_CLAMP] = sys_clamp;
    service[SYS_ABS] = sys_abs;
    service[SYS_MIN] = sys_min;
    service[SYS_MAX] = sys_max;
    service[SYS_BYTEORDER] = sys_byteorder;
}

void inspectStack()
{
    for (int i = 0; i < STACK_SIZE; i++) {
        char* arrow = &stack[i] == sp ? " <-" : "";
        int n = AS_INT(stack[i]);

        printf("%d: %d%s\n", i, n, arrow);
    }
}

static void resetStack()
{
    sp = stack;
    fp = stack;
}

static void run()
{
    uint8_t opcode;
    int32_t a;
    int32_t b;
    int32_t x;
    Function func = getFunctionAt(functions, 0);
    Value value;

    TEST_STACK_OVERFLOW(func.maxStackCount);

    while (opcode = READ_UINT8()) {
        switch (opcode)
        {
            case OP_SYSCALL:
                x = READ_UINT8();
                service[x]();
                break;

            case OP_LDC:
                x = READ_UINT8();
                value = getValueAt(constants, x);
                PUSH(value);
                break;

            case OP_LDG:
                x = READ_UINT8();
                value = getValueAt(globals, x);
                PUSH(value);
                break;

            case OP_STG:
                x = READ_UINT8();
                setValueAt(globals, x, sp[-1]);
                POP();
                break;

            case OP_LDL:
                x = (int8_t) READ_UINT8();
                PUSH(fp[x]);
                break;

            case OP_LDL_0:
                PUSH(fp[0]);
                break;

            case OP_LDL_1:
                PUSH(fp[1]);
                break;

            case OP_LDL_2:
                PUSH(fp[2]);
                break;

            case OP_LDL_3:
                PUSH(fp[3]);
                break;

            case OP_STL:
                x = (int8_t) READ_UINT8();
                fp[x] = POP();
                break;

            case OP_STL_0:
                fp[0] = POP();
                break;

            case OP_STL_1:
                fp[1] = POP();
                break;

            case OP_STL_2:
                fp[2] = POP();
                break;

            case OP_STL_3:
                fp[3] = POP();
                break;

            case OP_PUSHB:
                x = (int8_t) READ_UINT8();
                PUSH(INT_VALUE(x));
                break;

            case OP_PUSHH:
                x = (int16_t) READ_UINT16();
                PUSH(INT_VALUE(x));
                break;

            case OP_PUSH_N1:
                PUSH(INT_VALUE(-1));
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
                PUSH(sp[-1]);
                break;

            case OP_INC:
                AS_INT(sp[-1])++;
                break;

            case OP_DEC:
                AS_INT(sp[-1])--;
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
                x = AS_INT(sp[-1]);
                sp[-1] = INT_VALUE(~x);
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
                x = AS_INT(sp[-1]);
                sp[-1] = INT_VALUE(-x);
                break;

            case OP_NOT:
                x = AS_INT(sp[-1]);
                sp[-1] = INT_VALUE(!x);
                break;

            case OP_BEQ:
                b = AS_INT(sp[-1]);
                a = AS_INT(sp[-2]);
                if (a == b) pc += READ_UINT16();
                break;

            case OP_BLT:
                b = AS_INT(sp[-1]);
                a = AS_INT(sp[-2]);
                if (a < b) pc += READ_UINT16();
                break;

            case OP_BLE:
                b = AS_INT(sp[-1]);
                a = AS_INT(sp[-2]);
                if (a <= b) pc += READ_UINT16();
                break;

            case OP_JMP:
                pc += READ_UINT16();
                break;

            case OP_CALL:
                x = READ_UINT16();
                Function func = getFunctionAt(functions, x);
                
                TEST_STACK_OVERFLOW(func.maxStackCount);
                sp -= func.paramCount;
                memmove(sp + 2, sp, func.paramCount * sizeof(Value));
                PUSH(POINTER_VALUE(fp));
                PUSH(POINTER_VALUE(pc));
                fp = sp;
                sp += func.localCount;
                pc = &bc[func.position];
                break;

            case OP_RET:
                sp = fp - 2;
                pc = AS_POINTER(fp[-1]);
                fp = AS_POINTER(fp[-2]);
                PUSH(INT_VALUE(0));
                break;

            case OP_RETV:
                value = POP();
                sp = fp - 2;
                pc = AS_POINTER(fp[-1]);
                fp = AS_POINTER(fp[-2]);
                PUSH(value);
                break;

            default:
                return;
        }
    }
}

static void interpretChunk(Chunk* chunk)
{
    if (cargs.disassemble) {
        return disassemble(chunk);
    }

    bc = chunk->data;
    pc = chunk->data;
    functions = &chunk->functions;
    globals = &chunk->globals;
    constants = &chunk->constants;

    run();
}

void initVM()
{
    initServices();
    resetStack();
}

void interpret(char* source)
{
    Chunk chunk;
    initChunk(&chunk);
    compile(source, &chunk);
    interpretChunk(&chunk);
    freeChunk(&chunk);
}

#undef READ_UINT8
#undef READ_UINT16
