#ifndef VM_H
#define VM_H

#define INSTRUCTION_SIZE 40
#define STACK_SIZE 16
#define SYSCALL_SIZE 8

void initVM();
void interpret(char* source);
void inspectVM();

#endif
