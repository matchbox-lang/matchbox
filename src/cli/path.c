#include "cli/path.h"
#include "common/memory.h"
#include "common/util.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char* create_path(size_t length)
{
    return allocate_memory(length + 1);
}

static bool path_ends_with_separator(const char* path)
{
    size_t length = strlen(path);

    if (length == 0) {
        return false;
    }

    if (path[length - 1] == '\\') {
        return true;
    }

    return path[length - 1] == '/';
}

static bool path_needs_separator(const char* path)
{
    if (path[0] == '\0') {
        return false;
    }

    return !path_ends_with_separator(path);
}

static bool get_path_info(const char* path, struct stat* info)
{
    if (!path_ends_with_separator(path)) {
        return stat(path, info) == 0;
    }

    size_t length = strlen(path);

    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\') &&
        (length != 3 || path[1] != ':')) {
        length--;
    }

    char* normalized = create_path(length);

    memcpy(normalized, path, length);

    normalized[length] = '\0';
    int status = stat(normalized, info);

    free_memory(normalized);

    return status == 0;
}

char* join_path(const char* directory, const char* name)
{
    const char* separator = path_needs_separator(directory) ? "\\" : "";
    size_t length = strlen(directory) + strlen(separator) + strlen(name);
    char* path = create_path(length);

    snprintf(path, length + 1, "%s%s%s", directory, separator, name);

    return path;
}

void free_path(char* path)
{
    free_memory(path);
}

const char* path_for_display(const char* path)
{
    while (path[0] == '.' && (path[1] == '/' || path[1] == '\\')) {
        path += 2;
    }

    return path;
}

bool path_exists(const char* path)
{
    struct stat info;

    return get_path_info(path, &info);
}

bool path_has_extension(const char* path, const char* extension)
{
    size_t path_length = strlen(path);
    size_t extension_length = strlen(extension);

    if (path_length < extension_length) {
        return false;
    }

    const char* path_extension = path + path_length - extension_length;

    return strcmp(path_extension, extension) == 0;
}

bool path_is_current_or_parent_directory(const char* path)
{
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

bool path_is_directory(const char* path)
{
    struct stat info;

    return get_path_info(path, &info) && (info.st_mode & S_IFDIR);
}
