#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdbool.h>

#define PROGRAM_COMMAND "matchbox"
#define PROGRAM_VERSION "Matchbox 0.2.0"

typedef struct ProgramOptions
{
    bool disassemble;
    const char* filename;
} ProgramOptions;

void printUsage();
void initProgramOptions(ProgramOptions* options, int count, char* argv[]);

#endif
