#include "syscall.h"
#include "lexer.h"
#include <stdio.h>
#include <string.h>

static Syscall syscalls[SYSCALL_SIZE] = {
    {"exit", 0, 1, {T_INT}},
    {"print", 1, 1, {T_INT}}
};

Syscall* getSyscallByName(char* name)
{
    for (int i = 0; i < SYSCALL_SIZE; i++) {
        Syscall* syscall = &syscalls[i];

        if (strcmp(name, syscall->name) == 0) {
            return syscall;
        }
    }

    return NULL;
}
