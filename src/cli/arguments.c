#include "cli/arguments.h"
#include "cli/options.h"
#include "cli/program.h"
#include "common/errors.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void disassembleDoesNotSupportTestError()
{
    reportError("Cannot disassemble tests");
}

static void disassembleRequiresFileError()
{
    reportError("Cannot disassemble without a file");
}

static void disassembleDoesNotSupportCheckError()
{
    reportError("Cannot disassemble checks");
}

static void unexpectedArgumentError(const char* arg)
{
    reportError("Unexpected argument: %s", arg);
}

static void unknownOptionError(const char* arg)
{
    reportError("Unknown option: %s", arg);
}

static bool parseOption(Options* options, const char* arg)
{
    if (strcmp(arg, "--version") == 0) {
        printVersion();
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
        printUsage(stdout);
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arg, "-d") == 0 || strcmp(arg, "--disassemble") == 0) {
        options->disassemble = true;

        return true;
    }

    if (options->mode == PROGRAM_TEST && strcmp(arg, "-a") == 0) {
        options->testOutput = TEST_OUTPUT_ALL;

        return true;
    }

    if (options->mode == PROGRAM_TEST && strcmp(arg, "-p") == 0) {
        options->testOutput = TEST_OUTPUT_PASSED;

        return true;
    }

    unknownOptionError(arg);

    return false;
}

static bool isOption(const char* arg)
{
    return arg[0] == '-';
}

static bool isOptionDelimiter(const char* arg)
{
    return strcmp(arg, "--") == 0;
}

static bool isCommand(const Options* options, const char* arg, bool parsingOptions, const char* command)
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

    return strcmp(arg, command) == 0;
}

static bool parseTestArgument(Options* options, const char* arg)
{
    if (options->testPath) {
        unexpectedArgumentError(arg);

        return false;
    }

    options->testPath = arg;

    return true;
}

static bool parseRunArgument(Options* options, const char* arg)
{
    if (options->filename) {
        unexpectedArgumentError(arg);

        return false;
    }

    options->filename = arg;

    return true;
}

static bool parseArgument(Options* options, const char* arg, bool* parsingOptions)
{
    if (*parsingOptions && isOptionDelimiter(arg)) {
        *parsingOptions = false;

        return true;
    }

    if (*parsingOptions && isOption(arg)) {
        return parseOption(options, arg);
    }

    if (isCommand(options, arg, *parsingOptions, "check")) {
        options->mode = PROGRAM_CHECK;

        return true;
    }

    if (isCommand(options, arg, *parsingOptions, "test")) {
        options->mode = PROGRAM_TEST;

        return true;
    }

    if (options->mode == PROGRAM_TEST) {
        return parseTestArgument(options, arg);
    }

    return parseRunArgument(options, arg);
}

static bool parseArguments(Options* options, int argc, char* const argv[])
{
    bool parsingOptions = true;
    bool parsed = true;

    options->executablePath = argv[0];

    for (int i = 1; i < argc && parsed; i++) {
        parsed = parseArgument(options, argv[i], &parsingOptions);
    }

    return parsed;
}

bool parseCommandLine(Options* options, int argc, char* const argv[])
{
    if (!parseArguments(options, argc, argv)) {
        return false;
    }

    if (options->disassemble && options->mode == PROGRAM_CHECK) {
        disassembleDoesNotSupportCheckError();

        return false;
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
