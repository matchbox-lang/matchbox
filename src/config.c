#include "config.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

void usage()
{
    printf("Usage: %s <options> <file>\n", PROGRAM_COMMAND);
    exit(1);
}

static void parseOption(CommandArgs* args, char* arg)
{
    if (strcmp(arg, "--version") == 0) {
        fprintf(stdout, "%s\n", PROGRAM_VERSION);
        exit(0);
    }
    
    if (strcmp(arg, "-d") == 0) {
        args->disassemble = true;
    } else {
        fprintf(stderr, "Unknown option: %s\n", arg);
        usage();
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
