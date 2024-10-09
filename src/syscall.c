#include "syscall.h"
#include <stdio.h>
#include <string.h>

static char* syscalls[SYSCALL_SIZE] = {
    "exit",
    "print"
};

int getSyscall(char* name)
{
    for (int i = 0; i < SYSCALL_SIZE; i++) {
        if (strcmp(name, syscalls[i]) == 0) {
            return i;
        }
    }

    return -1;
}
