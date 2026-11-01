#include "cli/arguments.h"
#include "cli/options.h"
#include "cli/program.h"
#include "common/errors.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void disassemble_does_not_support_test_error()
{
    report_error("Cannot disassemble tests");
}

static void disassemble_requires_file_error()
{
    report_error("Cannot disassemble without a file");
}

static void disassemble_does_not_support_check_error()
{
    report_error("Cannot disassemble checks");
}

static void unexpected_argument_error(const char* arg)
{
    report_error("Unexpected argument: %s", arg);
}

static void unknown_option_error(const char* arg)
{
    report_error("Unknown option: %s", arg);
}

static bool parse_option(Options* options, const char* arg)
{
    if (strcmp(arg, "--version") == 0) {
        print_version();
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
        print_usage(stdout);
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arg, "-d") == 0 || strcmp(arg, "--disassemble") == 0) {
        options->disassemble = true;

        return true;
    }

    bool test_command = options->mode == ProgramTest;

    if (test_command && strcmp(arg, "--program") == 0) {
        options->programs = true;

        return true;
    }

    if (test_command && strcmp(arg, "-a") == 0) {
        options->test_output = TestOutputAll;

        return true;
    }

    if (test_command && strcmp(arg, "-p") == 0) {
        options->test_output = TestOutputPassed;

        return true;
    }

    unknown_option_error(arg);

    return false;
}

static bool is_option(const char* arg)
{
    return arg[0] == '-';
}

static bool is_option_delimiter(const char* arg)
{
    return strcmp(arg, "--") == 0;
}

static bool is_command(const Options* options, const char* arg, bool parsing_options, const char* command)
{
    if (!parsing_options) {
        return false;
    }

    if (options->mode != ProgramRun) {
        return false;
    }

    if (options->filename) {
        return false;
    }

    return strcmp(arg, command) == 0;
}

static bool parse_test_argument(Options* options, const char* arg)
{
    if (options->test_path) {
        unexpected_argument_error(arg);

        return false;
    }

    options->test_path = arg;

    return true;
}

static bool parse_run_argument(Options* options, const char* arg)
{
    if (options->filename) {
        unexpected_argument_error(arg);

        return false;
    }

    options->filename = arg;

    return true;
}

static bool parse_argument(Options* options, const char* arg, bool* parsing_options)
{
    if (*parsing_options && is_option_delimiter(arg)) {
        *parsing_options = false;

        return true;
    }

    if (*parsing_options && is_option(arg)) {
        return parse_option(options, arg);
    }

    if (is_command(options, arg, *parsing_options, "check")) {
        options->mode = ProgramCheck;

        return true;
    }

    if (is_command(options, arg, *parsing_options, "test")) {
        options->mode = ProgramTest;

        return true;
    }

    if (options->mode == ProgramTest) {
        return parse_test_argument(options, arg);
    }

    return parse_run_argument(options, arg);
}

static bool parse_arguments(Options* options, int argc, char* const argv[])
{
    bool parsing_options = true;
    bool parsed = true;

    options->executable_path = argv[0];

    for (int i = 1; i < argc && parsed; i++) {
        parsed = parse_argument(options, argv[i], &parsing_options);
    }

    return parsed;
}

bool parse_command_line(Options* options, int argc, char* const argv[])
{
    if (!parse_arguments(options, argc, argv)) {
        return false;
    }

    if (options->disassemble && options->mode == ProgramCheck) {
        disassemble_does_not_support_check_error();

        return false;
    }

    if (options->disassemble && options->mode == ProgramTest) {
        disassemble_does_not_support_test_error();

        return false;
    }


    if (options->disassemble && !options->filename) {
        disassemble_requires_file_error();

        return false;
    }

    return true;
}
