#include "config.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

void printUsage()
{
    printf("Usage: %s <options> <file>\n", PROGRAM_COMMAND);
    exit(1);
}

static void printUnknownOption(char* arg)
{
    fprintf(stderr, "Unknown option: %s\n", arg);
    printUsage();
}

static void printVersion()
{
    fprintf(stdout, "%s\n", PROGRAM_VERSION);
    exit(0);
}

static void parseOption(CommandArgs* args, char* arg)
{
    if (strcmp(arg, "--version") == 0) {
        printVersion();
    }
    
    if (strcmp(arg, "-d") == 0) {
        args->disassemble = true;
    } else {
        printUnknownOption(arg);
    }
}

void initCommandArgs(CommandArgs* args, int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            parseOption(args, argv[i]);
        } else {
            args->filename = argv[i];
        }
    }
}
