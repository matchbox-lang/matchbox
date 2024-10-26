#ifndef VM_H
#define VM_H

#define STACK_SIZE 32

void initVM();
void interpret(char* source);
void inspectVM();

#endif
