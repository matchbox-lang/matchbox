#ifndef SYSCALL_H
#define SYSCALL_H

#define SYSCALL_SIZE 7

typedef struct Syscall
{
    char* name;
    int opcode;
    int paramCount;
    int params[4];
    int typeId;
} Syscall;

typedef enum SyscallCode
{
    SYS_EXIT,
    SYS_PRINT,
    SYS_CLAMP,
    SYS_ABS,
    SYS_MIN,
    SYS_MAX,
    SYS_BYTEORDER
} SyscallCode;

Syscall* getSyscallByName(char* name);

#endif
