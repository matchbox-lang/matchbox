#ifndef VM_H
#define VM_H

void initVM();
void freeVM();
void interpret(char* source);
void inspectVM();

#endif
