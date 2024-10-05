#include "vm.h"
#include "bytecode.h"
#include "chunk.h"
#include "compiler.h"
#include "syscall.h"
#include "value.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READ_UINT16() (vm.pc += 2, (uint16_t)((vm.pc[-2] << 8) | vm.pc[-1]))
#define READ_UINT8() (*(vm.pc++))

#define SLOT_BOOL(i) (AS_BOOL(vm.stack[i]))
#define SLOT_FLOAT(i) (AS_FLOAT(vm.stack[i]))
#define SLOT_INT(i) (AS_INT(vm.stack[i]))
#define SLOT_POINTER(i) (AS_POINTER(vm.stack[i]))

typedef void (*syscall_t)();
typedef void (*instruction_t)();

typedef struct VM
{
    bool running;
    syscall_t syscode[SYSCALL_SIZE];
    instruction_t opcode[INSTRUCTION_SIZE];
    Value stack[STACK_SIZE];
    Value* sp;
    Value* fp;
    uint8_t* pc;
    uint8_t* ra;
    ValueArray* constants;
} VM;

VM vm;

static void error(const char* message)
{
    fprintf(stderr, message);
    exit(1);
}

static void errors(const char* message, const char* s)
{
    fprintf(stderr, message, s);
    exit(1);
}

static void push(Value value)
{
    vm.sp[0] = value;
    vm.sp++;
}

static Value pop()
{
    vm.sp--;
    return vm.sp[0];
}

static Value peek(size_t index)
{
    return vm.sp[-(index + 1)];
}

static void sys_exit()
{
    uint32_t status = AS_INT(peek(1));
    
    exit(status);
}

static void op_hlt()
{
    vm.running = false;
}

static void op_syscall()
{
    uint32_t n = AS_INT(peek(0));

    vm.syscode[n]();
}

static void op_ldc()
{
    int8_t imm = READ_UINT8();
    Value constant = vm.constants->values[imm];

    push(constant);
}

static void op_ldl()
{
    int8_t imm = READ_UINT8();
    Value local = vm.fp[imm];

    push(local);
}

static void op_ldl_0()
{
    push(vm.fp[0]);
}

static void op_ldl_1()
{
    push(vm.fp[1]);
}

static void op_ldl_2()
{
    push(vm.fp[2]);
}

static void op_stl()
{
    int8_t imm = READ_UINT8();

    vm.fp[imm] = peek(0);
}

static void op_stl_0()
{
    vm.fp[0] = peek(0);
}

static void op_stl_1()
{
    vm.fp[1] = peek(0);
}

static void op_stl_2()
{
    vm.fp[2] = peek(0);
}

static void op_push()
{
    int8_t imm = READ_UINT8();

    push(INT_VALUE(imm));
}

static void op_push_0()
{
    push(INT_VALUE(0));
}

static void op_push_1()
{
    push(INT_VALUE(1));
}

static void op_pop()
{
    pop();
}

static void op_add()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a + b));
}

static void op_sub()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a - b));
}

static void op_mul()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a * b));
}

static void op_div()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a / b));
}

static void op_rem()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a % b));
}

static void op_pow()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(pow(a, b)));
}

static void op_band()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a & b));
}

static void op_bor()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a | b));
}

static void op_bxor()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a ^ b));
}

static void op_bnot()
{
    uint32_t n = AS_INT(vm.sp[-1]);

    vm.sp[-1] = INT_VALUE(~n);
}

static void op_lsl()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a << b));
}

static void op_lsr()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(a >> b));
}

static void op_asr()
{
    uint32_t b = AS_INT(pop());
    uint32_t a = AS_INT(pop());

    push(INT_VALUE(~(~a >> b)));
}

static void op_abs()
{
    uint32_t n = AS_INT(pop());

    push(INT_VALUE(((n >> 31) | 1) * n));
}

static void op_not()
{
    uint32_t n = AS_INT(vm.sp[-1]);

    vm.sp[-1] = INT_VALUE(!n);
}

static void op_neg()
{
    uint32_t n = AS_INT(vm.sp[-1]);

    vm.sp[-1] = INT_VALUE(-n);
}

static void op_inc()
{
    AS_INT(vm.sp[-1])++;
}

static void op_dec()
{
    AS_INT(vm.sp[-1])--;
}

static void op_beq()
{
    int16_t imm = READ_UINT16();
    Value b = peek(0);
    Value a = peek(1);

    if (AS_INT(a) == AS_INT(b)) {
        vm.pc += imm;
    }
}

static void op_blt()
{
    int16_t imm = READ_UINT16();
    Value b = peek(0);
    Value a = peek(1);

    if (AS_INT(a) < AS_INT(b)) {
        vm.pc += imm;
    }
}

static void op_ble()
{
    int16_t imm = READ_UINT16();
    Value b = peek(0);
    Value a = peek(1);

    if (AS_INT(a) <= AS_INT(b)) {
        vm.pc += imm;
    }
}

static void op_jmp()
{
    vm.pc += READ_UINT16();
}

static void op_jsr()
{
    int16_t imm = READ_UINT16();
    Value ra = POINTER_VALUE(vm.pc);
    Value fp = POINTER_VALUE(vm.fp);

    push(ra);
    push(fp);

    vm.pc += imm;
    vm.fp = vm.sp;
}

static void op_ret()
{
    Value ra = vm.fp[-2];
    Value fp = vm.fp[-1];

    vm.sp = vm.fp - 2;
    vm.fp = AS_POINTER(fp);
    vm.pc = AS_POINTER(ra);
}

static void op_retv()
{
    Value value = pop();
    op_ret();
    push(value);
}

static void initOpcodes()
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
    vm.opcode[OP_ABS] = op_abs;
    vm.opcode[OP_NOT] = op_not;
    vm.opcode[OP_NEG] = op_neg;
    vm.opcode[OP_INC] = op_inc;
    vm.opcode[OP_DEC] = op_dec;
    vm.opcode[OP_BEQ] = op_beq;
    vm.opcode[OP_BLT] = op_blt;
    vm.opcode[OP_BLE] = op_ble;
    vm.opcode[OP_JMP] = op_jmp;
    vm.opcode[OP_JSR] = op_jsr;
    vm.opcode[OP_RET] = op_ret;
    vm.opcode[OP_RETV] = op_retv;
}

static void initSyscalls()
{
    vm.syscode[SYS_EXIT] = sys_exit;
}

static void resetStack()
{
    vm.sp = vm.stack;
    vm.fp = vm.stack;
}

void initVM()
{
    initOpcodes();
    initSyscalls();
    resetStack();
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
    vm.pc = chunk->code;
    run();
}

void interpret(char* source)
{
    Chunk chunk;
    initChunk(&chunk);
    compile(source, &chunk);
    disassemble(&chunk);
    initVM();
    interpretChunk(&chunk);
    inspectVM();
    freeChunk(&chunk);
}

void inspectVM()
{
    printf("\n");

    for (int i = 0; i < STACK_SIZE; i++) {
        printf("%d: %d\n", i, SLOT_INT(i));
    }
}

#undef READ_UINT8
#undef READ_UINT16
