#include "program.h"
#include <stdio.h>
#include <stdlib.h>

void printUsage()
{
    printf("Usage: %s <options> <file>\n", PROGRAM_COMMAND);
    exit(1);
}

void printVersion()
{
    fprintf(stdout, "%s\n", PROGRAM_VERSION);
    exit(0);
}
