#include "test_runner.h"
#include "options.h"
#include "path.h"
#include "program.h"
#include "util.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DEFAULT_PATH "tests"

typedef struct TestRun {
    TestOutput output;
    const char* executablePath;
    bool excludeFuture;
    int passed;
    int failed;
    int found;
} TestRun;

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static void runDirectoryTests(TestRun* run, const char* path);
static int countDirectoryTests(const TestRun* run, const char* path);

static void expectedFileError(const char* path)
{
    fprintf(stderr, "Error: Expected .mb file but found %s\n", path);
}

static void runTestError(const char* filename)
{
    fprintf(stderr, "Error: Could not run %s\n", filename);
}

static void testDirectoryError(const char* path)
{
    fprintf(stderr, "Error: Could not read directory %s\n", path);
}

static void testPathError(const char* path)
{
    fprintf(stderr, "Error: Could not find path %s\n", path);
}

static void testsNotFoundError(const char* path)
{
    fprintf(stderr, "Error: Could not find tests in %s\n", path);
}

static char* quoteArgument(const char* argument)
{
    size_t length = strlen(argument);
    char* quoted = malloc(length + 3);

    if (!quoted) {
        outOfMemoryError();
    }

    quoted[0] = '"';
    memcpy(quoted + 1, argument, length);
    quoted[length + 1] = '"';
    quoted[length + 2] = '\0';

    return quoted;
}

static void freeTestCommand(char* command, char* quotedExecutablePath, char* quotedFilename)
{
    free(command);
    free(quotedFilename);
    free(quotedExecutablePath);
}

static char* createTestCommand(const char* executablePath, const char* filename,
    char** quotedExecutablePath, char** quotedFilename)
{
    size_t commandLength;
    char* command;

    *quotedExecutablePath = quoteArgument(executablePath);
    *quotedFilename = quoteArgument(filename);
    commandLength = strlen(*quotedExecutablePath) + strlen(*quotedFilename) + 9;
    command = malloc(commandLength + 1);

    if (!command) {
        outOfMemoryError();
    }

    snprintf(command, commandLength + 1, "\"%s %s 2>&1\"", *quotedExecutablePath, *quotedFilename);

    return command;
}

static void discardProcessOutput(FILE* stream)
{
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        continue;
    }
}

static bool runTestCommand(const char* command, const char* filename, bool expectsFailure)
{
    FILE* pipe = popen(command, "r");
    int status;

    if (!pipe) {
        runTestError(filename);

        return false;
    }

    discardProcessOutput(pipe);
    status = pclose(pipe);

    return expectsFailure ? status != 0 : status == 0;
}

static bool shouldPrintTestResult(TestOutput output, bool passedTest)
{
    if (output == TEST_OUTPUT_ALL) {
        return true;
    }

    return (output == TEST_OUTPUT_PASSED) == passedTest;
}

static void printTestResult(TestOutput output, bool passedTest, const char* filename)
{
    if (!shouldPrintTestResult(output, passedTest)) {
        return;
    }

    if (passedTest) {
        printf("PASS %s\n", filename);

        return;
    }

    printf("FAIL %s\n", filename);
}

static bool runTestFile(const char* executablePath, const char* filename, TestOutput output)
{
    char* quotedExecutablePath;
    char* quotedFilename;
    char* command = createTestCommand(executablePath, filename, &quotedExecutablePath, &quotedFilename);
    bool expectsFailure = pathHasExtension(filename, ".fail.mb");
    bool passedTest = runTestCommand(command, filename, expectsFailure);

    freeTestCommand(command, quotedExecutablePath, quotedFilename);
    printTestResult(output, passedTest, filename);

    return passedTest;
}

static void countTestResult(TestRun* run, bool passedTest)
{
    if (passedTest) {
        run->passed++;
        
        return;
    }

    run->failed++;
}

static void runTestFilePath(TestRun* run, const char* path)
{
    bool passedTest;

    if (!pathHasExtension(path, ".mb")) {
        return;
    }

    run->found++;
    passedTest = runTestFile(run->executablePath, path, run->output);
    countTestResult(run, passedTest);
}

static char* getDirectoryEntryPath(const char* path, const char* name)
{
    if (pathIsCurrentOrParentDirectory(name)) {
        return NULL;
    }

    return joinPath(path, name);
}

static bool shouldExcludeDirectory(const TestRun* run, const char* path, const char* name)
{
    return run->excludeFuture && strcmp(path, TEST_DEFAULT_PATH) == 0 && strcmp(name, "future") == 0;
}

static int countDirectoryEntryTests(const TestRun* run, const char* path, const char* name)
{
    char* child = getDirectoryEntryPath(path, name);
    int count;

    if (!child) {
        return 0;
    }

    if (shouldExcludeDirectory(run, path, name)) {
        freePath(child);

        return 0;
    }

    if (!pathIsDirectory(child)) {
        count = pathHasExtension(child, ".mb") ? 1 : 0;
        freePath(child);

        return count;
    }

    count = countDirectoryTests(run, child);
    freePath(child);

    return count;
}

static int countDirectoryEntries(const TestRun* run, DIR* directory, const char* path)
{
    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(directory))) {
        int entryCount = countDirectoryEntryTests(run, path, entry->d_name);

        if (entryCount < 0) {
            return -1;
        }

        count += entryCount;
    }

    return count;
}

static int countDirectoryTests(const TestRun* run, const char* path)
{
    DIR* directory = opendir(path);
    int count;

    if (!directory) {
        testDirectoryError(path);

        return -1;
    }

    count = countDirectoryEntries(run, directory, path);
    closedir(directory);

    return count;
}

static void runDirectoryEntry(TestRun* run, const char* path, const char* name)
{
    char* child = getDirectoryEntryPath(path, name);

    if (!child) {
        return;
    }

    if (shouldExcludeDirectory(run, path, name)) {
        freePath(child);

        return;
    }

    if (pathIsDirectory(child)) {
        runDirectoryTests(run, child);
        freePath(child);

        return;
    }

    runTestFilePath(run, child);
    freePath(child);
}

static void runDirectoryTests(TestRun* run, const char* path)
{
    DIR* directory = opendir(path);
    struct dirent* entry;

    if (!directory) {
        testDirectoryError(path);
        run->failed++;

        return;
    }

    while ((entry = readdir(directory))) {
        runDirectoryEntry(run, path, entry->d_name);
    }

    closedir(directory);
}

static bool validateTestPath(const char* path)
{
    if (!pathExists(path)) {
        testPathError(path);

        return false;
    }

    if (!pathIsDirectory(path) && !pathHasExtension(path, ".mb")) {
        expectedFileError(path);

        return false;
    }

    return true;
}

static void runTestPath(TestRun* run, const char* path)
{
    if (pathIsDirectory(path)) {
        runDirectoryTests(run, path);

        return;
    }

    runTestFilePath(run, path);
}

static const char* configureTestRun(TestRun* run, const Options* options)
{
    if (options->executablePath) {
        run->executablePath = options->executablePath;
    }

    if (!options->testPath) {
        return TEST_DEFAULT_PATH;
    }

    run->excludeFuture = false;

    return options->testPath;
}

static int countTests(const TestRun* run, const char* path)
{
    if (pathIsDirectory(path)) {
        return countDirectoryTests(run, path);
    }

    return 1;
}

static bool validateTestCount(const char* path, int testCount)
{
    if (testCount < 0) {
        return false;
    }

    if (testCount > 0) {
        return true;
    }

    testsNotFoundError(path);

    return false;
}

static void printTestStart(int testCount)
{
    printf("Running %d %s...\n", testCount, testCount == 1 ? "test" : "tests");
    fflush(stdout);
}

static void printTestSummary(const TestRun* run)
{
    printf("%d %s passed, ", run->passed, run->passed == 1 ? "test" : "tests");
    printf("%d %s failed.\n", run->failed, run->failed == 1 ? "test" : "tests");
}

bool runTests(Options* options)
{
    TestRun run = {options->testOutput, PROGRAM_COMMAND, true, 0, 0, 0};
    const char* path = configureTestRun(&run, options);
    int testCount;

    if (!validateTestPath(path)) {
        return false;
    }

    testCount = countTests(&run, path);

    if (!validateTestCount(path, testCount)) {
        return false;
    }

    printTestStart(testCount);
    runTestPath(&run, path);

    if (run.found == 0) {
        testsNotFoundError(path);

        return false;
    }

    printTestSummary(&run);

    return run.failed == 0;
}
