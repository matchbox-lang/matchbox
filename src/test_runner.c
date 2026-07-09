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

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static void runDirectoryTests(const char* executablePath, const char* path, int* passed, int* failed, int* found);

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

static char* createTestCommand(const char* executablePath, const char* filename, char** quotedExecutablePath, char** quotedFilename)
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

static bool runTestCommand(const char* command, const char* filename)
{
    FILE* pipe = popen(command, "r");
    int status;

    if (!pipe) {
        runTestError(filename);

        return false;
    }

    discardProcessOutput(pipe);
    status = pclose(pipe);

    return status == 0;
}

static void printTestResult(bool passedTest, const char* filename)
{
    if (passedTest) {
        printf("PASS %s\n", filename);

        return;
    }

    printf("FAIL %s\n", filename);
}

static bool runTestFile(const char* executablePath, const char* filename)
{
    char* quotedExecutablePath;
    char* quotedFilename;
    char* command = createTestCommand(executablePath, filename, &quotedExecutablePath, &quotedFilename);
    bool passedTest = runTestCommand(command, filename);

    freeTestCommand(command, quotedExecutablePath, quotedFilename);
    printTestResult(passedTest, filename);

    return passedTest;
}

static void countTestResult(bool passedTest, int* passed, int* failed)
{
    if (passedTest) {
        (*passed)++;
        return;
    }

    (*failed)++;
}

static void runTestFilePath(const char* executablePath, const char* path, int* passed, int* failed, int* found)
{
    bool passedTest;

    if (!pathHasExtension(path, ".mb")) {
        return;
    }

    (*found)++;
    passedTest = runTestFile(executablePath, path);
    countTestResult(passedTest, passed, failed);
}

static char* getDirectoryEntryPath(const char* path, const char* name)
{
    if (pathIsCurrentOrParentDirectory(name)) {
        return NULL;
    }

    return joinPath(path, name);
}

static void runDirectoryEntry(const char* executablePath, const char* path, const char* name, int* passed, int* failed, int* found)
{
    char* child = getDirectoryEntryPath(path, name);

    if (!child) {
        return;
    }

    if (pathIsDirectory(child)) {
        runDirectoryTests(executablePath, child, passed, failed, found);
        free(child);

        return;
    }

    runTestFilePath(executablePath, child, passed, failed, found);
    free(child);
}

static void runDirectoryTests(const char* executablePath, const char* path, int* passed, int* failed, int* found)
{
    DIR* directory = opendir(path);
    struct dirent* entry;

    if (!directory) {
        testDirectoryError(path);
        (*failed)++;

        return;
    }

    while ((entry = readdir(directory))) {
        runDirectoryEntry(executablePath, path, entry->d_name, passed, failed, found);
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

static void runTestPath(const char* executablePath, const char* path, int* passed, int* failed, int* found)
{
    if (pathIsDirectory(path)) {
        runDirectoryTests(executablePath, path, passed, failed, found);

        return;
    }

    runTestFilePath(executablePath, path, passed, failed, found);
}

void runTests(Options* options)
{
    const char* path = TEST_DEFAULT_PATH;
    const char* executablePath = PROGRAM_COMMAND;
    int passed = 0;
    int failed = 0;
    int found = 0;

    if (options->testPath) {
        path = options->testPath;
    }

    if (options->executablePath) {
        executablePath = options->executablePath;
    }

    if (!validateTestPath(path)) {
        return;
    }

    runTestPath(executablePath, path, &passed, &failed, &found);

    if (found == 0) {
        testsNotFoundError(path);

        return;
    }

    printf("%d test(s) passed, ", passed);
    printf("%d test(s) failed.\n", failed);
}
