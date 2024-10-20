#ifndef VM_H
#define VM_H

#define INSTRUCTION_SIZE 42
#define STACK_SIZE 16

void initVM();
void interpret(char* source);
void inspectVM();

#endif
