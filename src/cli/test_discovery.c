#include "cli/test_discovery.h"
#include "cli/path.h"
#include "common/errors.h"
#include "common/memory.h"
#include "config.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

static int discover_test_path(Vector* paths, const char* path);

static int discover_test_directory_entry(Vector* paths, const char* path, const char* name)
{
    if (path_is_current_or_parent_directory(name)) {
        return 0;
    }

    char* child = join_path(path, name);
    int failures = discover_test_path(paths, child);

    free_path(child);

    return failures;
}

static int discover_test_directory(Vector* paths, const char* path)
{
    DIR* directory = opendir(path);

    if (!directory) {
        report_error("Could not read directory %s", path);

        return 1;
    }

    struct dirent* entry;
    int failures = 0;
    errno = 0;

    while ((entry = readdir(directory))) {
        failures += discover_test_directory_entry(paths, path, entry->d_name);
        errno = 0;
    }

    int read_error = errno;

    closedir(directory);

    if (read_error != 0) {
        failures++;

        report_error("Could not read directory %s", path);
    }

    return failures;
}

static int discover_test_path(Vector* paths, const char* path)
{
    if (path_is_directory(path)) {
        return discover_test_directory(paths, path);
    }

    if (!path_has_extension(path, PROGRAM_FILE_EXTENSION)) {
        return 0;
    }

    size_t length = strlen(path);
    char* copy = allocate_memory(length + 1);

    memcpy(copy, path, length + 1);
    push_vector_item(paths, copy);

    return 0;
}

static int compare_paths(const void* left, const void* right)
{
    const char* left_path = *(const char* const*)left;
    const char* right_path = *(const char* const*)right;

    return strcmp(left_path, right_path);
}

int discover_test_paths(Vector* paths, const char* path)
{
    int failures = discover_test_path(paths, path);

    qsort(paths->data, paths->count, sizeof(void*), compare_paths);

    return failures;
}
