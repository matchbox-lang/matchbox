#include "cli/test_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include "config.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DEFAULT_PATH "tests"

typedef struct TestRun {
    TestOutput output;
    const char* executable_path;
    int passed;
    int failed;
    int found;
} TestRun;

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

static void run_directory_tests(TestRun* run, const char* path);
static int count_directory_tests(const char* path);

static void expected_file_error(const char* path)
{
    report_error("Expected .mb file but found %s", path);
}

static void run_test_error(const char* path)
{
    report_error("Could not run %s", path);
}

static void test_directory_error(const char* path)
{
    report_error("Could not read directory %s", path);
}

static void test_path_error(const char* path)
{
    report_error("Could not find path %s", path);
}

static void tests_not_found_error(const char* path)
{
    report_error("Could not find tests in %s", path);
}

static char* quote_argument(const char* argument)
{
    size_t length = strlen(argument);
    char* quoted = allocate_memory(length + 3);

    quoted[0] = '"';
    memcpy(quoted + 1, argument, length);
    quoted[length + 1] = '"';
    quoted[length + 2] = '\0';

    return quoted;
}

static void free_test_command(char* command, char* quoted_executable_path, char* quoted_filename)
{
    free_memory(command);
    free_memory(quoted_filename);
    free_memory(quoted_executable_path);
}

static char* create_test_command(const char* executable_path, const char* filename,
    char** quoted_executable_path, char** quoted_filename)
{
    size_t command_length;
    char* command;

    *quoted_executable_path = quote_argument(executable_path);
    *quoted_filename = quote_argument(filename);
    command_length = strlen(*quoted_executable_path) + strlen(*quoted_filename) + 9;
    command = allocate_memory(command_length + 1);

    snprintf(command, command_length + 1, "\"%s %s 2>&1\"", *quoted_executable_path, *quoted_filename);

    return command;
}

static void discard_process_output(FILE* stream)
{
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        continue;
    }
}

static bool run_test_command(const char* command, const char* filename, bool expects_failure)
{
    FILE* pipe = POPEN(command, "r");
    int status;

    if (!pipe) {
        run_test_error(filename);

        return false;
    }

    discard_process_output(pipe);
    status = PCLOSE(pipe);

    return expects_failure ? status != 0 : status == 0;
}

static bool should_print_test_result(TestOutput output, bool passed_test)
{
    if (output == TestOutputAll) {
        return true;
    }

    return (output == TestOutputPassed) == passed_test;
}

static void print_test_result(TestOutput output, bool passed_test, const char* filename)
{
    if (!should_print_test_result(output, passed_test)) {
        return;
    }

    if (passed_test) {
        printf("PASS %s\n", filename);

        return;
    }

    printf("FAIL %s\n", filename);
}

static bool run_test_file(const char* executable_path, const char* filename, TestOutput output)
{
    char* quoted_executable_path;
    char* quoted_filename;
    char* command = create_test_command(executable_path, filename, &quoted_executable_path, &quoted_filename);
    bool expects_failure = path_has_extension(filename, ".fail.mb");
    bool passed_test = run_test_command(command, filename, expects_failure);

    free_test_command(command, quoted_executable_path, quoted_filename);
    print_test_result(output, passed_test, filename);

    return passed_test;
}

static void count_test_result(TestRun* run, bool passed_test)
{
    if (passed_test) {
        run->passed++;
        
        return;
    }

    run->failed++;
}

static void run_test_file_path(TestRun* run, const char* path)
{
    bool passed_test;

    if (!path_has_extension(path, ".mb")) {
        return;
    }

    run->found++;
    passed_test = run_test_file(run->executable_path, path, run->output);
    count_test_result(run, passed_test);
}

static char* get_directory_entry_path(const char* path, const char* name)
{
    if (path_is_current_or_parent_directory(name)) {
        return NULL;
    }

    return join_path(path, name);
}

static int count_directory_entry_tests(const char* path, const char* name)
{
    char* child = get_directory_entry_path(path, name);
    int count;

    if (!child) {
        return 0;
    }

    if (!path_is_directory(child)) {
        count = path_has_extension(child, ".mb") ? 1 : 0;
        free_path(child);

        return count;
    }

    count = count_directory_tests(child);
    free_path(child);

    return count;
}

static int count_directory_entries(DIR* directory, const char* path)
{
    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(directory))) {
        int entry_count = count_directory_entry_tests(path, entry->d_name);

        if (entry_count < 0) {
            return -1;
        }

        count += entry_count;
    }

    return count;
}

static int count_directory_tests(const char* path)
{
    DIR* directory = opendir(path);
    int count;

    if (!directory) {
        test_directory_error(path);

        return -1;
    }

    count = count_directory_entries(directory, path);
    closedir(directory);

    return count;
}

static void run_directory_entry(TestRun* run, const char* path, const char* name)
{
    char* child = get_directory_entry_path(path, name);

    if (!child) {
        return;
    }

    if (path_is_directory(child)) {
        run_directory_tests(run, child);
        free_path(child);

        return;
    }

    run_test_file_path(run, child);
    free_path(child);
}

static void run_directory_tests(TestRun* run, const char* path)
{
    DIR* directory = opendir(path);
    struct dirent* entry;

    if (!directory) {
        test_directory_error(path);
        run->failed++;

        return;
    }

    while ((entry = readdir(directory))) {
        run_directory_entry(run, path, entry->d_name);
    }

    closedir(directory);
}

static bool validate_test_path(const char* path)
{
    if (!path_exists(path)) {
        test_path_error(path);

        return false;
    }

    if (!path_is_directory(path) && !path_has_extension(path, ".mb")) {
        expected_file_error(path);

        return false;
    }

    return true;
}

static void run_test_path(TestRun* run, const char* path)
{
    if (path_is_directory(path)) {
        run_directory_tests(run, path);

        return;
    }

    run_test_file_path(run, path);
}

static const char* configure_test_run(TestRun* run, const Options* options)
{
    if (options->executable_path) {
        run->executable_path = options->executable_path;
    }

    if (!options->test_path) {
        return TEST_DEFAULT_PATH;
    }

    return options->test_path;
}

static int count_tests(const char* path)
{
    if (path_is_directory(path)) {
        return count_directory_tests(path);
    }

    return 1;
}

static bool validate_test_count(const char* path, int test_count)
{
    if (test_count < 0) {
        return false;
    }

    if (test_count > 0) {
        return true;
    }

    tests_not_found_error(path);

    return false;
}

static void print_test_start(int test_count)
{
    printf("Running %d %s...\n", test_count, test_count == 1 ? "test" : "tests");
    fflush(stdout);
}

static void print_test_summary(const TestRun* run)
{
    printf("%d %s passed, ", run->passed, run->passed == 1 ? "test" : "tests");
    printf("%d %s failed.\n", run->failed, run->failed == 1 ? "test" : "tests");
}

int run_tests(const Options* options)
{
    TestRun run = {options->test_output, PROGRAM_COMMAND, 0, 0, 0};
    const char* path = configure_test_run(&run, options);
    int test_count;

    if (!validate_test_path(path)) {
        return EXIT_FAILURE;
    }

    test_count = count_tests(path);

    if (!validate_test_count(path, test_count)) {
        return EXIT_FAILURE;
    }

    print_test_start(test_count);
    run_test_path(&run, path);

    if (run.found == 0) {
        tests_not_found_error(path);

        return EXIT_FAILURE;
    }

    print_test_summary(&run);

    return run.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
