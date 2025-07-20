#ifndef VM_H
#define VM_H

#include "moduleobject.h"
#include "service.h"
#include <stdint.h>

#define STACK_MAX 1024
#define FRAMES_MAX 64

typedef struct VM VM;
typedef Value (*service_t)(int argCount, Value* args);

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

void initVM(VM* vm, ModuleObject* module);
void freeVM(VM* vm);
void inspectVM(VM* vm);
void interpret(VM* vm);

#endif
