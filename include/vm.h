#ifndef VM_H
#define VM_H

#include "module_object.h"
#include "opcode.h"
#include <stdint.h>

#define STACK_MAX 1024

typedef struct VM
{
    Value stack[STACK_MAX];
    Instruction* ip;
    Value* sp;
    Value* fp;
    Value* gp;
    ModuleObject* module;
} VM;

void initVM(VM* vm, ModuleObject* module);
void freeVM(VM* vm);
void inspectStack(VM* vm);
void interpret(VM* vm);

#endif
