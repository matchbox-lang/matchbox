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
    bool exit_requested;
    int exit_status;
    jmp_buf exit_jump;
} VM;

void init_vm(VM* vm, ModuleObject* module);
void free_vm(VM* vm);
int interpret(VM* vm);
_Noreturn void exit_vm(VM* vm, int status);

#endif
