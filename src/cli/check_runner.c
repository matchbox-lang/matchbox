#include "cli/check_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "cli/runner.h"
#include "common/errors.h"
#include "config.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>

static bool check_path(const char* path);

static bool check_source_file(const void* data, MemoryGroup* result_memory)
{
    const char* path = data;
    char* source = read_source(path);

    (void)result_memory;
    analyze_source(source);

    return true;
}

static bool check_file(const char* path)
{
    bool success = run_isolated(check_source_file, path);

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

    if (!path_has_extension(path, PROGRAM_FILE_EXTENSION)) {
        return true;
    }

    return check_file(path);
}

int run_checks(const Options* options)
{
    const char* path = options->filename ? options->filename : ".";

    if (!validate_source_path(path)) {
        return EXIT_FAILURE;
    }

    bool success = check_path(path);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
