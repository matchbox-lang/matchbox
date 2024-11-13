#ifndef VM_H
#define VM_H

typedef struct CommandArgs CommandArgs;

void initVM();
void interpret(char* source, CommandArgs* args);
void inspectVM();

#endif
