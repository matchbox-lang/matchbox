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

#define PUSH(value) (vm.sp[0] = value, vm.sp++)
#define POP() ((--vm.sp)[0])
#define READ_UINT16() (vm.pc += 2, (uint16_t)((vm.pc[-2] << 8) | vm.pc[-1]))
#define READ_UINT8() (*(vm.pc++))

typedef void (*service_t)();
typedef void (*instruction_t)();

typedef struct VM
{
    bool running;
    instruction_t opcode[OP_SIZE];
    service_t service[SERVICE_SIZE];
    Value stack[STACK_SIZE];
    Value* sp;
    Value* fp;
    uint8_t* bc;
    uint8_t* pc;
    uint8_t* ra;
    FunctionArray* functions;
    ValueArray* constants;
} VM;

VM vm;

static void op_hlt()
{
    vm.running = false;
}

static void op_syscall()
{
    int32_t opcode = AS_INT(POP());

    vm.service[opcode]();
}

static void op_ldc()
{
    int8_t n = READ_UINT8();
    Value constant = getValueAt(vm.constants, n);

    PUSH(constant);
}

static void op_ldl()
{
    int8_t n = READ_UINT8();

    PUSH(vm.fp[n]);
}

static void op_ldl_0()
{
    PUSH(vm.fp[0]);
}

static void op_ldl_1()
{
    PUSH(vm.fp[1]);
}

static void op_ldl_2()
{
    PUSH(vm.fp[2]);
}

static void op_stl()
{
    int8_t n = READ_UINT8();

    vm.fp[n] = vm.sp[-1];
}

static void op_stl_0()
{
    vm.fp[0] = vm.sp[-1];
}

static void op_stl_1()
{
    vm.fp[1] = vm.sp[-1];
}

static void op_stl_2()
{
    vm.fp[2] = vm.sp[-1];
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

static void op_inc()
{
    int8_t n = READ_UINT8();

    AS_INT(vm.fp[n])++;
}

static void op_dec()
{
    int8_t n = READ_UINT8();

    AS_INT(vm.fp[n])--;
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
    int32_t n = AS_INT(vm.sp[-1]);

    vm.sp[-1] = INT_VALUE(~n);
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
    int32_t n = AS_INT(vm.sp[-1]);

    vm.sp[-1] = INT_VALUE(!n);
}

static void op_neg()
{
    int32_t n = AS_INT(vm.sp[-1]);

    vm.sp[-1] = INT_VALUE(-n);
}

static void op_beq()
{
    int16_t n = READ_UINT16();
    Value b = vm.sp[-1];
    Value a = vm.sp[-2];

    if (AS_INT(a) == AS_INT(b)) {
        vm.pc += n;
    }
}

static void op_blt()
{
    int16_t n = READ_UINT16();
    Value b = vm.sp[-1];
    Value a = vm.sp[-2];

    if (AS_INT(a) < AS_INT(b)) {
        vm.pc += n;
    }
}

static void op_ble()
{
    int16_t n = READ_UINT16();
    Value b = vm.sp[-1];
    Value a = vm.sp[-2];

    if (AS_INT(a) <= AS_INT(b)) {
        vm.pc += n;
    }
}

static void op_jmp()
{
    vm.pc += READ_UINT16();
}

static void op_call()
{
    int16_t n = READ_UINT16();
    Function func = getFunctionAt(vm.functions, n);
    Value sp = POINTER_VALUE(vm.sp - func.paramCount);
    Value ra = POINTER_VALUE(vm.pc);
    Value fp = POINTER_VALUE(vm.fp);

    PUSH(sp);
    PUSH(ra);
    PUSH(fp);

    vm.pc = &vm.bc[func.position];
    vm.fp = vm.sp;
    vm.sp += func.localCount;
}

static void op_ret()
{
    vm.sp = AS_POINTER(vm.fp[-3]);
    vm.pc = AS_POINTER(vm.fp[-2]);
    vm.fp = AS_POINTER(vm.fp[-1]);

    op_push_0();
}

static void op_retv()
{
    Value value = POP();

    vm.sp = AS_POINTER(vm.fp[-3]);
    vm.pc = AS_POINTER(vm.fp[-2]);
    vm.fp = AS_POINTER(vm.fp[-1]);

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
    int32_t tmp = num < min ? min : num;

    PUSH(INT_VALUE(tmp > max ? max : tmp));
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
    vm.opcode[OP_HLT] = op_hlt;
    vm.opcode[OP_SYSCALL] = op_syscall;
    vm.opcode[OP_LDC] = op_ldc;
    vm.opcode[OP_LDL] = op_ldl;
    vm.opcode[OP_LDL_0] = op_ldl_0;
    vm.opcode[OP_LDL_1] = op_ldl_1;
    vm.opcode[OP_LDL_2] = op_ldl_2;
    vm.opcode[OP_STL] = op_stl;
    vm.opcode[OP_STL_0] = op_stl_0;
    vm.opcode[OP_STL_1] = op_stl_1;
    vm.opcode[OP_STL_2] = op_stl_2;
    vm.opcode[OP_PUSH] = op_push;
    vm.opcode[OP_PUSH_0] = op_push_0;
    vm.opcode[OP_PUSH_1] = op_push_1;
    vm.opcode[OP_PUSH_2] = op_push_2;
    vm.opcode[OP_POP] = op_pop;
    vm.opcode[OP_ADD] = op_add;
    vm.opcode[OP_SUB] = op_sub;
    vm.opcode[OP_MUL] = op_mul;
    vm.opcode[OP_DIV] = op_div;
    vm.opcode[OP_REM] = op_rem;
    vm.opcode[OP_POW] = op_pow;
    vm.opcode[OP_BAND] = op_band;
    vm.opcode[OP_BOR] = op_bor;
    vm.opcode[OP_BXOR] = op_bxor;
    vm.opcode[OP_BNOT] = op_bnot;
    vm.opcode[OP_LSL] = op_lsl;
    vm.opcode[OP_LSR] = op_lsr;
    vm.opcode[OP_ASR] = op_asr;
    vm.opcode[OP_NOT] = op_not;
    vm.opcode[OP_NEG] = op_neg;
    vm.opcode[OP_INC] = op_inc;
    vm.opcode[OP_DEC] = op_dec;
    vm.opcode[OP_BEQ] = op_beq;
    vm.opcode[OP_BLT] = op_blt;
    vm.opcode[OP_BLE] = op_ble;
    vm.opcode[OP_JMP] = op_jmp;
    vm.opcode[OP_CALL] = op_call;
    vm.opcode[OP_RET] = op_ret;
    vm.opcode[OP_RETV] = op_retv;
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

static void resetStack()
{
    vm.sp = vm.stack;
    vm.fp = vm.stack;
}

void initVM()
{
    resetStack();
    initInstructions();
    initServices();
}

static void run()
{
    vm.running = true;

    while (vm.running) {
        uint8_t c = READ_UINT8();

        vm.opcode[c]();
    }
}

static void interpretChunk(Chunk* chunk)
{
    vm.bc = chunk->data;
    vm.pc = chunk->data;
    vm.constants = &chunk->constants;
    vm.functions = &chunk->functions;

    run();
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

void inspectVM()
{
    for (int i = 0; i < STACK_SIZE; i++) {
        char* arrow = &vm.stack[i] == vm.sp ? " <-" : "";
        int n = AS_INT(vm.stack[i]);

        printf("%d: %d%s\n", i, n, arrow);
    }
}

#undef READ_UINT8
#undef READ_UINT16
