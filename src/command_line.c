#include "command_line.h"
#include "options.h"
#include "program.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void disassembleDoesNotSupportTestError()
{
    fprintf(stderr, "Error: -d cannot be used with test\n");
}

static void disassembleRequiresFileError()
{
    fprintf(stderr, "Error: -d requires a file\n");
}

static void unknownOptionError(char* arg)
{
    fprintf(stderr, "Error: Unknown option: %s\n", arg);
}

static void unexpectedArgumentError(char* arg)
{
    fprintf(stderr, "Error: Unexpected argument: %s\n", arg);
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

static bool isOption(char* arg)
{
    return arg[0] == '-';
}

static bool isOptionDelimiter(char* arg)
{
    return strcmp(arg, "--") == 0;
}

static bool isTestCommand(Options* options, char* arg, bool parsingOptions)
{
    if (!parsingOptions) {
        return false;
    }

    if (options->mode != PROGRAM_RUN) {
        return false;
    }

    if (options->filename) {
        return false;
    }

    return strcmp(arg, "test") == 0;
}

static bool parseTestArgument(Options* options, char* arg)
{
    if (options->testPath) {
        unexpectedArgumentError(arg);

        return false;
    }

    options->testPath = arg;

    return true;
}

static bool parseRunArgument(Options* options, char* arg)
{
    if (options->filename) {
        unexpectedArgumentError(arg);

        return false;
    }

    options->filename = arg;

    return true;
}

static bool parseArgument(Options* options, char* arg, bool* parsingOptions)
{
    if (*parsingOptions && isOptionDelimiter(arg)) {
        *parsingOptions = false;

        return true;
    }

    if (*parsingOptions && isOption(arg)) {
        return parseOption(options, arg);
    }

    if (isTestCommand(options, arg, *parsingOptions)) {
        options->mode = PROGRAM_TEST;

        return true;
    }

    if (options->mode == PROGRAM_TEST) {
        return parseTestArgument(options, arg);
    }

    return parseRunArgument(options, arg);
}

bool parseCommandLine(Options* options, int argc, char* argv[])
{
    bool parsingOptions = true;

    options->executablePath = argv[0];

    for (int i = 1; i < argc; i++) {
        if (!parseArgument(options, argv[i], &parsingOptions)) {
            return false;
        }
    }

    if (options->disassemble && options->mode == PROGRAM_TEST) {
        disassembleDoesNotSupportTestError();

        return false;
    }

    if (options->disassemble && !options->filename) {
        disassembleRequiresFileError();

        return false;
    }

    return true;
}
