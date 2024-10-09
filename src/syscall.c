#include "syscall.h"
#include "lexer.h"
#include <stdio.h>
#include <string.h>

static Syscall syscalls[] = {
    {"exit", 0, 1, {T_INT}, T_NONE},
    {"print", 1, 1, {T_INT}, T_NONE},
    {"clamp", 2, 3, {T_INT, T_INT, T_INT}, T_INT},
    {"abs", 3, 1, {T_INT}, T_INT},
    {"min", 4, 2, {T_INT, T_INT}, T_INT},
    {"max", 5, 2, {T_INT, T_INT}, T_INT},
    {"byteorder", 6, 0, {}, T_INT}
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
