#ifndef VM_H
#define VM_H

#include "functionobject.h"

#define STACK_MAX 1024
#define FRAMES_MAX 64

void initVM();
void freeVM();
void interpret(FunctionObject* function);
void inspectVM();

#endif
