#include "command_line.h"
#include "options.h"
#include "program.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void unknownOptionError(char* arg)
{
    fprintf(stderr, "Error: Unknown option: %s\n", arg);
}

static void unexpectedArgumentError(char* arg)
{
    fprintf(stderr, "Error: Unexpected argument: %s\n", arg);
}

static void disassembleRequiresFileError()
{
    fprintf(stderr, "Error: -d requires a file\n");
}

static bool parseOption(Options* options, char* arg)
{
    if (strcmp(arg, "--version") == 0) {
        printVersion();
        exit(0);
    }

    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
        printUsage(stdout);
        exit(0);
    }

    if (strcmp(arg, "-d") == 0 || strcmp(arg, "--disassemble") == 0) {
        options->disassemble = true;

        return true;
    }

    unknownOptionError(arg);

    return false;
}

static bool parseArgument(Options* options, char* arg, bool* parsingOptions)
{
    if (*parsingOptions && strcmp(arg, "--") == 0) {
        *parsingOptions = false;

        return true;
    }

    if (*parsingOptions && arg[0] == '-') {
        return parseOption(options, arg);
    }

    if (options->filename) {
        unexpectedArgumentError(arg);

        return false;
    }

    options->filename = arg;

    return true;
}

bool parseCommandLine(Options* options, int argc, char* argv[])
{
    bool parsingOptions = true;

    for (int i = 1; i < argc; i++) {
        if (!parseArgument(options, argv[i], &parsingOptions)) {
            return false;
        }
    }

    if (options->disassemble && !options->filename) {
        disassembleRequiresFileError();

        return false;
    }

    return true;
}
