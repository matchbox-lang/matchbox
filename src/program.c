#include "program.h"

void printUsage(FILE* stream)
{
    fprintf(stream, "Usage: %s [options] [--] [file]\n", PROGRAM_COMMAND);
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --disassemble     Print bytecode without running the program\n");
    fprintf(stream, "  -h, --help            Show help options\n");
    fprintf(stream, "      --version         Show version information\n");
}

void printVersion()
{
    printf("%s\n", PROGRAM_VERSION);
}
