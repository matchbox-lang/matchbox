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

static bool check_path(const char* path);

static bool check_source_file(const char* path)
{
    char* source = get_file_contents(path);

    if (!source) {
        const char* detail = strerror(errno);

        report_error("Could not read file %s: %s", path, detail);

        return false;
    }

    Scope* scope = create_scope(NULL);
    ASTNode* ast = create_compound_node(scope);
    Parser parser;
    Analyzer analyzer;

    init_parser(&parser, ast);
    init_analyzer(&analyzer, ast);

    if (!parse(&parser, source)) {
        unexpected_end_of_input_error(parser.current_token);
    }

    return analyze(&analyzer, 0);
}

static bool check_file_with_context(const char* path, MemoryGroup* memory)
{
    ErrorContext context;

    begin_error_context(&context, memory);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);

        return false;
    }

    bool success = check_source_file(path);

    end_error_context(&context);

    return success;
}

static bool check_file(const char* path)
{
    MemoryGroup memory = {0};
    bool success = check_file_with_context(path, &memory);

    free_memory_group(&memory);

    if (!success) {
        report_error("Check failed for %s", path);
    }

    return success;
}

static bool check_directory_entry(const char* path, const char* name)
{
    if (path_is_current_or_parent_directory(name)) {
        return true;
    }

    char* child = join_path(path, name);
    bool success = check_path(child);

    free_path(child);

    return success;
}

static bool check_directory_entries(DIR* directory, const char* path)
{
    bool success = true;
    struct dirent* entry;

    errno = 0;

    while ((entry = readdir(directory))) {
        bool checked = check_directory_entry(path, entry->d_name);

        success = checked && success;
        errno = 0;
    }

    if (errno != 0) {
        report_error("Could not read directory %s", path);

        return false;
    }

    return success;
}

static bool check_directory(const char* path)
{
    DIR* directory = opendir(path);

    if (!directory) {
        report_error("Could not read directory %s", path);

        return false;
    }

    bool success = check_directory_entries(directory, path);

    closedir(directory);

    return success;
}

static bool check_path(const char* path)
{
    if (path_is_directory(path)) {
        return check_directory(path);
    }

    if (!path_has_extension(path, ".mb")) {
        return true;
    }

    return check_file(path);
}

int run_checks(const Options* options)
{
    const char* path = options->filename ? options->filename : ".";

    if (!path_exists(path)) {
        report_error("Could not find path %s", path);

        return EXIT_FAILURE;
    }

    if (!path_is_directory(path) && !path_has_extension(path, ".mb")) {
        report_error("Expected .mb file but found %s", path);

        return EXIT_FAILURE;
    }

    bool success = check_path(path);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
