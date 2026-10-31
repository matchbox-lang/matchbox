#include "cli/test_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "cli/program.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DEFAULT_PATH "tests"

typedef struct TestRun {
    TestOutput output;
    const char* executablePath;
    int passed;
    int failed;
    int found;
} TestRun;

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static void runDirectoryTests(TestRun* run, const char* path);
static int countDirectoryTests(const char* path);

static void expectedFileError(const char* path)
{
    reportError("Expected .mb file but found %s", path);
}

static void runTestError(const char* path)
{
    reportError("Could not run %s", path);
}

static void testDirectoryError(const char* path)
{
    reportError("Could not read directory %s", path);
}

static void testPathError(const char* path)
{
    reportError("Could not find path %s", path);
}

static void testsNotFoundError(const char* path)
{
    reportError("Could not find tests in %s", path);
}

static char* quoteArgument(const char* argument)
{
    size_t length = strlen(argument);
    char* quoted = allocateMemory(length + 3);

    quoted[0] = '"';
    memcpy(quoted + 1, argument, length);
    quoted[length + 1] = '"';
    quoted[length + 2] = '\0';

    return quoted;
}

static void freeTestCommand(char* command, char* quotedExecutablePath, char* quotedFilename)
{
    freeMemory(command);
    freeMemory(quotedFilename);
    freeMemory(quotedExecutablePath);
}

static char* createTestCommand(const char* executablePath, const char* filename,
    char** quotedExecutablePath, char** quotedFilename)
{
    size_t commandLength;
    char* command;

    *quotedExecutablePath = quoteArgument(executablePath);
    *quotedFilename = quoteArgument(filename);
    commandLength = strlen(*quotedExecutablePath) + strlen(*quotedFilename) + 9;
    command = allocateMemory(commandLength + 1);

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

static int countDirectoryEntryTests(const char* path, const char* name)
{
    char* child = getDirectoryEntryPath(path, name);
    int count;

    if (!child) {
        return 0;
    }

    if (!pathIsDirectory(child)) {
        count = pathHasExtension(child, ".mb") ? 1 : 0;
        freePath(child);

        return count;
    }

    count = countDirectoryTests(child);
    freePath(child);

    return count;
}

static int countDirectoryEntries(DIR* directory, const char* path)
{
    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(directory))) {
        int entryCount = countDirectoryEntryTests(path, entry->d_name);

        if (entryCount < 0) {
            return -1;
        }

        count += entryCount;
    }

    return count;
}

static int countDirectoryTests(const char* path)
{
    DIR* directory = opendir(path);
    int count;

    if (!directory) {
        testDirectoryError(path);

        return -1;
    }

    count = countDirectoryEntries(directory, path);
    closedir(directory);

    return count;
}

static void runDirectoryEntry(TestRun* run, const char* path, const char* name)
{
    char* child = getDirectoryEntryPath(path, name);

    if (!child) {
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

    return options->testPath;
}

static int countTests(const char* path)
{
    if (pathIsDirectory(path)) {
        return countDirectoryTests(path);
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
    TestRun run = {options->testOutput, PROGRAM_COMMAND, 0, 0, 0};
    const char* path = configureTestRun(&run, options);
    int testCount;

    if (!validateTestPath(path)) {
        return false;
    }

    testCount = countTests(path);

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
