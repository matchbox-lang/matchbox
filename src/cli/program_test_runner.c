#include "cli/program_test_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "cli/runner.h"
#include "cli/source_discovery.h"
#include "cli/test_output.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct ProgramTestRun
{
    Vector paths;
    TestOutput output;
    int passed;
    int failed;
    int discovery_failed;
} ProgramTestRun;

static bool run_program_file(const void* data, MemoryGroup* result_memory)
{
    const char* path = data;
    Options options;

    (void)result_memory;
    init_options(&options);

    options.filename = path;

    int status = run_program(&options);

    return status == EXIT_SUCCESS;
}

static bool should_print_result(TestOutput output, bool passed)
{
    if (output == TestOutputAll) {
        return true;
    }

    return (output == TestOutputPassed) == passed;
}

static void print_program_test_result(const ProgramTestRun* run, const char* path, FILE* output, bool passed)
{
    if (run->output == TestOutputSkipped && passed) {
        return;
    }

    if (!should_print_result(run->output, passed)) {
        return;
    }

    const char* display_path = strip_relative_path_prefix(path);

    if (passed) {
        printf("PASS %s\n", display_path);

        return;
    }

    FILE* stream = stdout;

    if (run->output == TestOutputSkipped) {
        stream = stderr;
    }

    fprintf(stream, "FAIL %s\n", display_path);
    print_test_output(output, stream, NULL);
}

static bool run_program_test_file(const ProgramTestRun* run, const char* path)
{
    int saved_stdout;
    int saved_stderr;
    FILE* output = begin_test_output_capture(&saved_stdout, &saved_stderr);

    if (!output) {
        return false;
    }

    bool success = run_isolated(run_program_file, path);
    bool flushed = end_test_output_capture(saved_stdout, &saved_stderr);
    bool expects_failure = path_has_extension(path, ".fail" PROGRAM_FILE_EXTENSION);
    bool passed = flushed && success != expects_failure;

    print_program_test_result(run, path, output, passed);
    fclose(output);

    return passed;
}

static void run_program_test_case(ProgramTestRun* run, const char* path)
{
    bool passed = run_program_test_file(run, path);

    if (passed) {
        run->passed++;

        return;
    }

    run->failed++;
}

static void free_program_test_run(ProgramTestRun* run)
{
    for (size_t i = 0; i < run->paths.count; i++) {
        char* path = run->paths.data[i];

        free_memory(path);
    }

    free_vector(&run->paths);
}

static const char* get_program_test_path(const Options* options)
{
    if (options->test_path) {
        return options->test_path;
    }

    return ".";
}

static void init_program_test_run(ProgramTestRun* run, const Options* options)
{
    *run = (ProgramTestRun){.output = options->test_output};

    init_vector(&run->paths);
}

static bool discover_program_tests(ProgramTestRun* run, const char* path)
{
    run->discovery_failed += discover_source_paths(&run->paths, path);

    if (run->paths.count == 0) {
        report_error("No programs found in %s", path);

        return false;
    }

    return true;
}

static void run_program_test_cases(ProgramTestRun* run)
{
    printf("Running %lu test programs...\n", (unsigned long)run->paths.count);
    fflush(stdout);

    for (size_t i = 0; i < run->paths.count; i++) {
        const char* filename = run->paths.data[i];

        run_program_test_case(run, filename);
    }
}

static int report_program_test_run(const ProgramTestRun* run)
{
    printf("%d passed; %d failed; 0 skipped.\n", run->passed, run->failed);

    if (run->failed > 0 || run->discovery_failed > 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int run_program_tests(const Options* options)
{
    const char* path = get_program_test_path(options);

    if (!validate_source_path(path)) {
        return EXIT_FAILURE;
    }

    ProgramTestRun run;

    init_program_test_run(&run, options);

    bool discovered = discover_program_tests(&run, path);

    if (!discovered) {
        free_program_test_run(&run);

        return EXIT_FAILURE;
    }

    run_program_test_cases(&run);

    int status = report_program_test_run(&run);

    free_program_test_run(&run);

    return status;
}
