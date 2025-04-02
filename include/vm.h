#ifndef VM_H
#define VM_H

#include "moduleobject.h"

#define STACK_MAX 1024
#define FRAMES_MAX 64

void initVM();
void freeVM();
void interpret(ModuleObject* module);
void inspectVM();

#endif
