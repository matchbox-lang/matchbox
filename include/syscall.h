#ifndef SYSCALL_H
#define SYSCALL_H

#define SYSCALL_SIZE 2

typedef struct Syscall
{
    char* name;
    int opcode;
    int paramCount;
    int params[4];
} Syscall;

typedef enum SyscallCode
{
    SYS_EXIT,
    SYS_PRINT
} SyscallCode;

Syscall* getSyscallByName(char* name);

#endif
