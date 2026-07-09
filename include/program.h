#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdbool.h>
#include <stdio.h>

#define PROGRAM_COMMAND "matchbox"
#define PROGRAM_VERSION "Matchbox 0.3.0"

typedef enum ProgramMode
{
    PROGRAM_RUN,
    PROGRAM_TEST
} ProgramMode;

typedef struct Options Options;

void printUsage(FILE* stream);
void printVersion();
bool runProgram(Options* options);

#endif
