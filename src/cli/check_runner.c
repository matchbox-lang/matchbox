#include "cli/check_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "common/errors.h"
#include "common/io.h"
#include "common/memory.h"
#include "compiler/analyzer.h"
#include "compiler/diagnostics.h"
#include "compiler/parser.h"
#include "compiler/scope.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool checkPath(const char* path);

static bool checkSourceFile(const char* path)
{
    char* source = getFileContents(path);

    if (!source) {
        const char* detail = strerror(errno);

        reportError("Could not read file %s: %s", path, detail);

        return false;
    }

    Scope* scope = createScope(NULL);
    ASTNode* ast = createCompoundNode(scope);
    Parser parser;
    Analyzer analyzer;

    initParser(&parser, ast);
    initAnalyzer(&analyzer, ast);

    if (!parse(&parser, source)) {
        unexpectedEndOfInputError(parser.currentToken);
    }

    return analyze(&analyzer, 0);
}

static bool checkFileWithContext(const char* path, MemoryGroup* memory)
{
    ErrorContext context;

    beginErrorContext(&context, memory);

    if (setjmp(context.jump) != 0) {
        endErrorContext(&context);

        return false;
    }

    bool success = checkSourceFile(path);

    endErrorContext(&context);

    return success;
}

static bool checkFile(const char* path)
{
    MemoryGroup memory = {0};
    bool success = checkFileWithContext(path, &memory);

    freeMemoryGroup(&memory);

    if (!success) {
        reportError("Check failed for %s", path);
    }

    return success;
}

static bool checkDirectoryEntry(const char* path, const char* name)
{
    if (pathIsCurrentOrParentDirectory(name)) {
        return true;
    }

    char* child = joinPath(path, name);
    bool success = checkPath(child);

    freePath(child);

    return success;
}

static bool checkDirectoryEntries(DIR* directory, const char* path)
{
    bool success = true;
    struct dirent* entry;

    errno = 0;

    while ((entry = readdir(directory))) {
        bool checked = checkDirectoryEntry(path, entry->d_name);

        success = checked && success;
        errno = 0;
    }

    if (errno != 0) {
        reportError("Could not read directory %s", path);

        return false;
    }

    return success;
}

static bool checkDirectory(const char* path)
{
    DIR* directory = opendir(path);

    if (!directory) {
        reportError("Could not read directory %s", path);

        return false;
    }

    bool success = checkDirectoryEntries(directory, path);

    closedir(directory);

    return success;
}

static bool checkPath(const char* path)
{
    if (pathIsDirectory(path)) {
        return checkDirectory(path);
    }

    if (!pathHasExtension(path, ".mb")) {
        return true;
    }

    return checkFile(path);
}

int runChecks(const Options* options)
{
    const char* path = options->filename ? options->filename : ".";

    if (!pathExists(path)) {
        reportError("Could not find path %s", path);

        return EXIT_FAILURE;
    }

    if (!pathIsDirectory(path) && !pathHasExtension(path, ".mb")) {
        reportError("Expected .mb file but found %s", path);

        return EXIT_FAILURE;
    }

    bool success = checkPath(path);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
