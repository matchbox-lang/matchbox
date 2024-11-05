#include "vm.h"
#include "bytecode.h"
#include "chunk.h"
#include "compiler.h"
#include "function.h"
#include "service.h"
#include "value.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUSH(value) (sp[0] = value, sp++)
#define POP() ((--sp)[0])
#define READ_UINT16() (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))
#define READ_UINT8() (*(pc++))

typedef void (*service_t)();
typedef void (*instruction_t)();

static bool running;
static instruction_t opcode[OP_SIZE];
static service_t service[SERVICE_SIZE];
static Value stack[STACK_SIZE];
static Value* sp;
static Value* fp;
static uint8_t* pc;
static uint8_t* bc;
static FunctionArray* functions;
static ValueArray* globals;

static void op_hlt()
{
    running = false;
}

static void op_syscall()
{
    int8_t n = READ_UINT8();

    service[n]();
}

static void op_ldg()
{
    int8_t n = READ_UINT8();
    Value global = getValueAt(globals, n);

    PUSH(global);
}

static void op_stg()
{
    int8_t n = READ_UINT8();

    setValueAt(globals, n, sp[-1]);
    POP();
}

static void op_ldl()
{
    int8_t n = READ_UINT8();

    PUSH(fp[n]);
}

static void op_ldl_0()
{
    PUSH(fp[0]);
}

static void op_ldl_1()
{
    PUSH(fp[1]);
}

static void op_ldl_2()
{
    PUSH(fp[2]);
}

static void op_stl()
{
    int8_t n = READ_UINT8();

    fp[n] = sp[-1];
    POP();
}

static void op_stl_0()
{
    fp[0] = sp[-1];
    POP();
}

static void op_stl_1()
{
    fp[1] = sp[-1];
    POP();
}

static void op_stl_2()
{
    fp[2] = sp[-1];
    POP();
}

static void op_push()
{
    int8_t n = READ_UINT8();

    PUSH(INT_VALUE(n));
}

static void op_push_0()
{
    PUSH(INT_VALUE(0));
}

static void op_push_1()
{
    PUSH(INT_VALUE(1));
}

static void op_push_2()
{
    PUSH(INT_VALUE(2));
}

static void op_pop()
{
    POP();
}

static void op_dup()
{
    PUSH(sp[-1]);
}

static void op_inc()
{
    AS_INT(sp[-1])++;
}

static void op_dec()
{
    AS_INT(sp[-1])--;
}

static void op_add()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a + b));
}

static void op_sub()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a - b));
}

static void op_mul()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a * b));
}

static void op_div()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a / b));
}

static void op_rem()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a % b));
}

static void op_pow()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(pow(a, b)));
}

static void op_band()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a & b));
}

static void op_bor()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a | b));
}

static void op_bxor()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a ^ b));
}

static void op_bnot()
{
    int32_t n = AS_INT(sp[-1]);

    sp[-1] = INT_VALUE(~n);
}

static void op_lsl()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a << b));
}

static void op_lsr()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(a >> b));
}

static void op_asr()
{
    int32_t b = AS_INT(POP());
    int32_t a = AS_INT(POP());

    PUSH(INT_VALUE(~(~a >> b)));
}

static void op_not()
{
    int32_t n = AS_INT(sp[-1]);

    sp[-1] = INT_VALUE(!n);
}

static void op_neg()
{
    int32_t n = AS_INT(sp[-1]);

    sp[-1] = INT_VALUE(-n);
}

static void op_beq()
{
    int16_t n = READ_UINT16();
    Value b = sp[-1];
    Value a = sp[-2];

    if (AS_INT(a) == AS_INT(b)) {
        pc += n;
    }
}

static void op_blt()
{
    int16_t n = READ_UINT16();
    Value b = sp[-1];
    Value a = sp[-2];

    if (AS_INT(a) < AS_INT(b)) {
        pc += n;
    }
}

static void op_ble()
{
    int16_t n = READ_UINT16();
    Value b = sp[-1];
    Value a = sp[-2];

    if (AS_INT(a) <= AS_INT(b)) {
        pc += n;
    }
}

static void op_jmp()
{
    pc += READ_UINT16();
}

static void op_call()
{
    int16_t n = READ_UINT16();
    Function func = getFunctionAt(functions, n);
    
    Value pcx = POINTER_VALUE(pc);
    Value fpx = POINTER_VALUE(fp);
    Value spx = POINTER_VALUE(sp - func.paramCount);

    PUSH(pcx);
    PUSH(spx);
    PUSH(fpx);

    pc = &bc[func.position];
    fp = sp;
    sp += func.localCount;
}

static void op_ret()
{
    pc = AS_POINTER(fp[-3]);
    sp = AS_POINTER(fp[-2]);
    fp = AS_POINTER(fp[-1]);

    op_push_0();
}

static void op_retv()
{
    Value value = POP();

    pc = AS_POINTER(fp[-3]);
    sp = AS_POINTER(fp[-2]);
    fp = AS_POINTER(fp[-1]);

    PUSH(value);
}

static void sys_exit()
{
    int32_t status = AS_INT(POP());
    
    exit(status);
}

static void sys_print()
{
    int32_t n = AS_INT(POP());
    
    printf("%d\n", n);
    op_push_0();
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
    char *c = (char*)&i;

    PUSH(INT_VALUE((int32_t)*c));
}

static void initInstructions()
{
    opcode[OP_HLT] = op_hlt;
    opcode[OP_SYSCALL] = op_syscall;
    opcode[OP_LDG] = op_ldg;
    opcode[OP_STG] = op_stg;
    opcode[OP_LDL] = op_ldl;
    opcode[OP_LDL_0] = op_ldl_0;
    opcode[OP_LDL_1] = op_ldl_1;
    opcode[OP_LDL_2] = op_ldl_2;
    opcode[OP_STL] = op_stl;
    opcode[OP_STL_0] = op_stl_0;
    opcode[OP_STL_1] = op_stl_1;
    opcode[OP_STL_2] = op_stl_2;
    opcode[OP_PUSH] = op_push;
    opcode[OP_PUSH_0] = op_push_0;
    opcode[OP_PUSH_1] = op_push_1;
    opcode[OP_PUSH_2] = op_push_2;
    opcode[OP_POP] = op_pop;
    opcode[OP_DUP] = op_dup;
    opcode[OP_ADD] = op_add;
    opcode[OP_SUB] = op_sub;
    opcode[OP_MUL] = op_mul;
    opcode[OP_DIV] = op_div;
    opcode[OP_REM] = op_rem;
    opcode[OP_POW] = op_pow;
    opcode[OP_BAND] = op_band;
    opcode[OP_BOR] = op_bor;
    opcode[OP_BXOR] = op_bxor;
    opcode[OP_BNOT] = op_bnot;
    opcode[OP_LSL] = op_lsl;
    opcode[OP_LSR] = op_lsr;
    opcode[OP_ASR] = op_asr;
    opcode[OP_NOT] = op_not;
    opcode[OP_NEG] = op_neg;
    opcode[OP_INC] = op_inc;
    opcode[OP_DEC] = op_dec;
    opcode[OP_BEQ] = op_beq;
    opcode[OP_BLT] = op_blt;
    opcode[OP_BLE] = op_ble;
    opcode[OP_JMP] = op_jmp;
    opcode[OP_CALL] = op_call;
    opcode[OP_RET] = op_ret;
    opcode[OP_RETV] = op_retv;
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

static void resetStack()
{
    sp = stack;
    fp = stack;
}

void initVM()
{
    resetStack();
    initInstructions();
    initServices();
}

static void run()
{
    running = true;

    while (running) {
        uint8_t c = READ_UINT8();

        opcode[c]();
    }
}

static void interpretChunk(Chunk* chunk)
{
    bc = chunk->data;
    pc = chunk->data;
    functions = &chunk->functions;
    globals = &chunk->globals;

    run();
}

void inspectStack()
{
    for (int i = 0; i < STACK_SIZE; i++) {
        char* arrow = &stack[i] == sp ? " <-" : "";
        int n = AS_INT(stack[i]);

        printf("%d: %d%s\n", i, n, arrow);
    }
}

void interpret(char* source)
{
    Chunk chunk;
    initChunk(&chunk);
    compile(source, &chunk);
    initVM();
    interpretChunk(&chunk);
    freeChunk(&chunk);
}

#undef READ_UINT8
#undef READ_UINT16
