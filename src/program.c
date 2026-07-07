#include "program.h"
#include <stdio.h>
#include <stdlib.h>

void printUsage(FILE* stream, int status)
{
    fprintf(stream, "Usage: %s [options] [--] [file]\n", PROGRAM_COMMAND);
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --disassemble     Print bytecode without running the program\n");
    fprintf(stream, "  -h, --help            Show help options\n");
    fprintf(stream, "      --version         Show version information\n");

    exit(status);
}

void printVersion()
{
    fprintf(stdout, "%s\n", PROGRAM_VERSION);
    exit(0);
}
