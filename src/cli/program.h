#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdbool.h>
#include <stdio.h>

typedef enum ProgramMode
{
    ProgramRun,
    ProgramCheck,
    ProgramTest
} ProgramMode;

typedef struct Options Options;

void print_usage(FILE* stream);
void print_version();
int run_program(const Options* options);

#endif
