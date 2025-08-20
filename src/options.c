#include "options.h"
#include "program.h"
#include "string.h"
#include <stdbool.h>
#include <stdio.h>

static void printUnknownOption(char* arg)
{
    fprintf(stderr, "Unknown option: %s\n", arg);
    printUsage();
}

static void parseOption(Options* options, char* arg)
{
    if (strcmp(arg, "--version") == 0) {
        printVersion();
    }
    
    if (strcmp(arg, "-d") == 0) {
        options->disassemble = true;
    } else {
        printUnknownOption(arg);
    }
}

void initOptions(Options* options, int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            parseOption(options, argv[i]);
        } else {
            options->filename = argv[i];
        }
    }
}
