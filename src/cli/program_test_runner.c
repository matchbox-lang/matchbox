#include "cli/program_test_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <sys/wait.h>
#define POPEN popen
#define PCLOSE pclose
#endif

typedef struct ProgramTestRun
{
    Vector paths;
    const char* executable_path;
    TestOutput output;
    int passed;
    int failed;
    int discovery_failed;
} ProgramTestRun;

static void discover_program_test_path(ProgramTestRun* run, const char* path);

static void append_process_output(Vector* output, const char* buffer)
{
    size_t length = strlen(buffer);
    char* copy = allocate_memory(length + 1);

    memcpy(copy, buffer, length + 1);
    push_vector_item(output, copy);
}

static bool read_process_output(FILE* stream, Vector* output)
{
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), stream)) {
        append_process_output(output, buffer);
    }

    return ferror(stream) == 0;
}

static void free_process_output(Vector* output)
{
    for (size_t i = 0; i < output->count; i++) {
        char* buffer = output->data[i];

        free_memory(buffer);
    }

    free_vector(output);
}

static bool process_crashed(int status)
{
#ifdef _WIN32
    return status < 0;
#else
    return !WIFEXITED(status);
#endif
}

static bool check_process_result(int status, bool expects_failure)
{
    if (status == -1 || process_crashed(status)) {
        return false;
    }

    int exit_status = status;

#ifndef _WIN32
    exit_status = WEXITSTATUS(status);
#endif

    if (expects_failure) {
        return exit_status != EXIT_SUCCESS;
    }

    return exit_status == EXIT_SUCCESS;
}

static bool should_print_result(TestOutput output, bool passed)
{
    if (output == TestOutputAll) {
        return true;
    }

    return (output == TestOutputPassed) == passed;
}

static void print_process_output(const Vector* output)
{
    for (size_t i = 0; i < output->count; i++) {
        const char* buffer = output->data[i];

        fputs(buffer, stdout);
    }
}

static void print_program_test_result(const ProgramTestRun* run, const char* path, int status, const Vector* output,
    bool passed)
{
    if (!should_print_result(run->output, passed)) {
        return;
    }

    const char* display_path = path_for_display(path);

    if (passed) {
        printf("PASS %s\n", display_path);

        return;
    }

    printf("FAIL %s (process status %d)\n", display_path, status);
    print_process_output(output);
}

static char* create_program_test_command(const char* executable_path, const char* path)
{
    size_t executable_length = strlen(executable_path);
    size_t path_length = strlen(path);
    size_t command_length = executable_length + path_length + 32;
    char* command = allocate_memory(command_length);

#ifdef _WIN32
    snprintf(command, command_length, "\"\"%s\" -- \"%s\" 2>&1\"", executable_path, path);
#else
    snprintf(command, command_length, "exec \"%s\" -- \"%s\" 2>&1", executable_path, path);
#endif

    return command;
}

static bool run_program_test_file(const ProgramTestRun* run, const char* path)
{
    char* command = create_program_test_command(run->executable_path, path);
    FILE* stream = POPEN(command, "r");

    free_memory(command);

    if (!stream) {
        report_error("Could not run program test %s", path);

        return false;
    }

    Vector output;

    init_vector(&output);

    bool read_success = read_process_output(stream, &output);
    int status = PCLOSE(stream);
    bool expects_failure = path_has_extension(path, ".fail.mb");
    bool passed = read_success && check_process_result(status, expects_failure);

    print_program_test_result(run, path, status, &output, passed);
    free_process_output(&output);

    return passed;
}

static void count_program_test_result(ProgramTestRun* run, const char* path)
{
    bool passed = run_program_test_file(run, path);

    if (passed) {
        run->passed++;

        return;
    }

    run->failed++;
}

static void discover_program_test_entry(ProgramTestRun* run, const char* path, const char* name)
{
    if (path_is_current_or_parent_directory(name)) {
        return;
    }

    char* child = join_path(path, name);

    discover_program_test_path(run, child);
    free_path(child);
}

static void discover_program_test_directory(ProgramTestRun* run, const char* path)
{
    DIR* directory = opendir(path);

    if (!directory) {
        run->discovery_failed++;

        report_error("Could not read directory %s", path);

        return;
    }

    struct dirent* entry;

    errno = 0;

    while ((entry = readdir(directory))) {
        discover_program_test_entry(run, path, entry->d_name);

        errno = 0;
    }

    int read_error = errno;

    closedir(directory);

    if (read_error != 0) {
        run->discovery_failed++;

        report_error("Could not read directory %s", path);
    }
}

static void discover_program_test_path(ProgramTestRun* run, const char* path)
{
    if (path_is_directory(path)) {
        discover_program_test_directory(run, path);

        return;
    }

    if (!path_has_extension(path, ".mb")) {
        return;
    }

    size_t length = strlen(path);
    char* copy = allocate_memory(length + 1);

    memcpy(copy, path, length + 1);
    push_vector_item(&run->paths, copy);
}

static int compare_paths(const void* left, const void* right)
{
    const char* left_path = *(const char* const*)left;
    const char* right_path = *(const char* const*)right;

    return strcmp(left_path, right_path);
}

static void free_program_test_run(ProgramTestRun* run)
{
    for (size_t i = 0; i < run->paths.count; i++) {
        char* path = run->paths.data[i];

        free_memory(path);
    }

    free_vector(&run->paths);
}

static bool validate_program_test_path(const char* path)
{
    if (!path_exists(path)) {
        report_error("Could not find path %s", path);

        return false;
    }

    if (!path_is_directory(path) && !path_has_extension(path, ".mb")) {
        report_error("Expected .mb file but found %s", path);

        return false;
    }

    return true;
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
    *run = (ProgramTestRun){.output = options->test_output, .executable_path = options->executable_path};

    if (!run->executable_path) {
        run->executable_path = PROGRAM_COMMAND;
    }

    init_vector(&run->paths);
}

static bool discover_program_tests(ProgramTestRun* run, const char* path)
{
    discover_program_test_path(run, path);
    qsort(run->paths.data, run->paths.count, sizeof(void*), compare_paths);

    if (run->paths.count == 0) {
        report_error("No programs found in %s", path);

        return false;
    }

    return true;
}

static void run_program_test_cases(ProgramTestRun* run)
{
    printf("Running %lu program tests...\n", (unsigned long)run->paths.count);
    fflush(stdout);

    for (size_t i = 0; i < run->paths.count; i++) {
        const char* filename = run->paths.data[i];

        count_program_test_result(run, filename);
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

    if (!validate_program_test_path(path)) {
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
