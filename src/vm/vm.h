#ifndef VM_H
#define VM_H

#include "runtime/module_object.h"
#include "vm/opcode.h"
#include <setjmp.h>

#define STACK_MAX 1024

typedef struct VM
{
    Value stack[STACK_MAX];
    Instruction* ip;
    Value* fp;
    ValueArray globals;
    ModuleObject* module;
    bool exitRequested;
    int exitStatus;
    jmp_buf exitJump;
} VM;

void initVM(VM* vm, ModuleObject* module);
void freeVM(VM* vm);
int interpret(VM* vm);
_Noreturn void exitVM(VM* vm, int status);

#endif
