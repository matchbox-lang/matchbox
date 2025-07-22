#ifndef VM_H
#define VM_H

#include "moduleobject.h"
#include "service.h"
#include <stdint.h>

#define STACK_MAX 1024

typedef Value (*service_t)(Value* args);

typedef struct VM
{
    Value stack[STACK_MAX];
    service_t service[SERVICES_MAX];
    uint8_t* ip;
    Value* sp;
    Value* fp;
    ModuleObject* module;
    ValueArray globals;
} VM;

void initVM(VM* vm, ModuleObject* module);
void freeVM(VM* vm);
void inspectVM(VM* vm);
void interpret(VM* vm);

#endif
