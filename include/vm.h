#ifndef VM_H
#define VM_H

#include "builtin.h"
#include "module_object.h"
#include <stdint.h>

#define STACK_MAX 1024

typedef Value (*builtin_t)(Value* args);

typedef struct VM
{
    Value stack[STACK_MAX];
    builtin_t builtins[BUILTINS_MAX];
    uint8_t* ip;
    Value* sp;
    Value* fp;
    ModuleObject* module;
    ValueArray globals;
} VM;

void initVM(VM* vm, ModuleObject* module);
void freeVM(VM* vm);
void inspectStack(VM* vm);
void interpret(VM* vm);

#endif
