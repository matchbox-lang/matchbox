#include "cli/check_runner.h"
#include "cli/options.h"
#include "cli/runner.h"
#include "cli/source_discovery.h"
#include "common/errors.h"
#include "common/memory.h"
#include <stdbool.h>
#include <stdlib.h>

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

static bool check_source_paths(Vector* paths)
{
    bool success = true;

    for (size_t i = 0; i < paths->count; i++) {
        char* path = paths->data[i];
        bool checked = check_file(path);

        free_memory(path);

        success = checked && success;
    }

    return success;
}

int run_checks(const Options* options)
{
    const char* path = options->filename ? options->filename : ".";

    if (!validate_source_path(path)) {
        return EXIT_FAILURE;
    }

    Vector paths;

    init_vector(&paths);

    int failures = discover_source_paths(&paths, path);
    bool success = check_source_paths(&paths);

    free_vector(&paths);

    return success && failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
