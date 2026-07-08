#ifndef PROGRAM_H
#define PROGRAM_H

#include "options.h"
#include <stdio.h>

#define PROGRAM_COMMAND "matchbox"
#define PROGRAM_VERSION "Matchbox 0.3.0"

void printUsage(FILE* stream);
void printVersion();
void runProgram(Options* options);

#endif
